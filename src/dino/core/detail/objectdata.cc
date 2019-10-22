// Copyright (c) 2018 Akihito Takeuchi
// Distributed under the MIT License : http://opensource.org/licenses/MIT

#include "dino/core/detail/objectdata.h"

#include <fstream>
#include <unordered_set>
#include <boost/lexical_cast.hpp>
#include <boost/filesystem.hpp>
#include <boost/range/iterator_range.hpp>

#include "dino/core/session.h"
#include "dino/core/dobject.h"
#include "dino/core/dexception.h"
#include "dino/core/currentuser.h"
#include "dino/core/commandstack.h"
#include "dino/core/objectfactory.h"
#include "dino/core/detail/dataiofactory.h"
#include "dino/core/detail/objectdataexception.h"

#define THROW1(code, info)                        \
  BOOST_THROW_EXCEPTION(ObjectDataException(code) \
                        << ExpInfo1(info));

#define THROW2(code, info1, info2) \
  BOOST_THROW_EXCEPTION(ObjectDataException(code) \
                        << ExpInfo1(info1) \
                        << ExpInfo2(info2));
  


namespace dino {

namespace core {

namespace fs = boost::filesystem;
namespace ipc = boost::interprocess;

namespace detail {

namespace {

const std::string kDataSectionName = "data";
const char* kLockFileSuffix = ".lock";
const unsigned int k_edit_type_mask =
    static_cast<unsigned int>(CommandType::kEditTypeMask);
const unsigned int k_command_group_mask =
    static_cast<unsigned int>(CommandType::kValueUpdateType)
    | static_cast<unsigned int>(CommandType::kBaseObjectUpdateType)
    | static_cast<unsigned int>(CommandType::kChildListUpdateType);

class BaseObjInfo {
 public:
  BaseObjInfo() = default;
  BaseObjInfo(const DObjPath& p,
              const DObjectSp& o)
      : path_(p), obj_(o) {}
  std::string Name() const { return path_.LeafName(); }
  DObjPath Path() const { return path_; }
  DObjectSp Obj() const { return obj_; }
  void SetObj(const DObjectSp& o) { obj_ = o; }
  std::vector<boost::signals2::connection>& Connections() {
    return connections_;
  }
  const std::vector<boost::signals2::connection>& Connections() const {
    return connections_;
  }
 private:
  DObjPath path_;
  DObjectSp obj_;
  std::vector<boost::signals2::connection> connections_;
};

void Remove(const fs::path& path) {
  try {
    fs::remove(path);
  } catch (const fs::filesystem_error&) {
  }
}

void CleanUpObjectDirectory(const fs::path& dir_path) {
  auto dirs = boost::make_iterator_range(
      fs::directory_iterator(dir_path),
      fs::directory_iterator());
  for (auto dir_itr : dirs) {
    auto child = dir_itr.path();
    if (fs::is_directory(child)) {
      CleanUpObjectDirectory(child);
      Remove(child);
    } else {
      auto info = DataIOFactory::GetDataFileInfo(child.string());
      if (info.IsValid())
        Remove(child);
    }
  }
  Remove(dir_path);
}

const std::string kBaseObjPathKeyBase = "__base_";
const std::string kBaseObjCountKey = "__base_count";

void StoreBaseToDict(const std::vector<BaseObjInfo>& base_info_list,
                     DValueDict& attrs) {
  for (size_t idx = 0; idx < base_info_list.size(); ++ idx) {
    auto key = kBaseObjPathKeyBase + boost::lexical_cast<std::string>(idx);
    attrs[key] = base_info_list[idx].Path().String();
  }
  attrs[kBaseObjCountKey] = static_cast<int>(base_info_list.size());
}

void RemoveBaseFromDict(DValueDict& attrs) {
  auto base_count = boost::get<int>(attrs[kBaseObjCountKey]);
  for (int idx = 0; idx < base_count; ++ idx) {
    auto key = kBaseObjPathKeyBase + boost::lexical_cast<std::string>(idx);
    attrs.erase(key);
  }
  attrs.erase(kBaseObjCountKey);
}

void RestoreBaseFromDict(DValueDict& attrs,
                         std::vector<BaseObjInfo>& base_info_list) {
  base_info_list.clear();
  auto itr = attrs.find(kBaseObjCountKey);
  if (itr == attrs.end())
    return;
  auto base_count = boost::get<int>(attrs[kBaseObjCountKey]);
  for (int idx = 0; idx < base_count; ++ idx) {
    auto key = kBaseObjPathKeyBase + boost::lexical_cast<std::string>(idx);
    base_info_list.emplace_back(
        BaseObjInfo(DObjPath(boost::get<std::string>(attrs[key])), nullptr));
    attrs.erase(key);
  }
  attrs.erase(kBaseObjCountKey);
}

template<typename T, typename F>
T ExecInObjWithKey(std::vector<BaseObjInfo*>& base_info_list,
                   const F& func_info) {
  for (auto& base_info : base_info_list) {
    if (base_info->Obj()->HasKey(func_info.key))
      return func_info.then(*base_info);
  }
  return func_info.else_();
}

using DObjCompareFunc = std::function<
  bool (const DObjInfo& lhs, const DObjInfo& rhs)>;

void SortDObjInfoVector(std::vector<DObjInfo>& obj_list,
                        const DObjCompareFunc& comp,
                        bool enable_sorting) {
  if (enable_sorting)
    std::sort(obj_list.begin(),
              obj_list.end(),
              [&comp](const auto& lhs, const auto& rhs) {
                return comp(lhs, rhs); });
}

template<typename T>
class FindObjInfo {
 public:
  FindObjInfo(std::vector<T>& obj_list) : obj_list_(obj_list) {}
  ~FindObjInfo() = default;
  FindObjInfo<T>& ByPath(const DObjPath& path) {
    itr_ = std::find_if(
        obj_list_.begin(), obj_list_.end(),
        [&path] (auto& obj) { return obj.Path() == path; });
    return *this;
  }
  FindObjInfo<T>& ByName(const std::string& name) {
    itr_ = std::find_if(
        obj_list_.begin(), obj_list_.end(),
        [&name] (auto& obj) { return obj.Name() == name; });
    return *this;
  }
  T& Obj() {
    return *itr_;
  }
  typename std::vector<T>::iterator Itr() {
    return itr_;
  }
  bool Found() const {
    return itr_ != obj_list_.end();
  }
 private:
  std::vector<T>& obj_list_;
  typename std::vector<T>::iterator itr_;
};

template<>
class FindObjInfo<BaseObjInfo*> {
 public:
  FindObjInfo(std::vector<BaseObjInfo*>& obj_list) : obj_list_(obj_list) {}
  ~FindObjInfo() = default;
  FindObjInfo<BaseObjInfo*>& ByPath(const DObjPath& path) {
    itr_ = std::find_if(
        obj_list_.begin(), obj_list_.end(),
        [&path] (auto& obj) { return obj->Path() == path; });
    return *this;
  }
  FindObjInfo<BaseObjInfo*>& ByName(const std::string& name) {
    itr_ = std::find_if(
        obj_list_.begin(), obj_list_.end(),
        [&name] (auto& obj) { return obj->Name() == name; });
    return *this;
  }
  BaseObjInfo& Obj() {
    return **itr_;
  }
  std::vector<BaseObjInfo*>::iterator Itr() {
    return itr_;
  }
  bool Found() const {
    return itr_ != obj_list_.end();
  }
 private:
  std::vector<BaseObjInfo*>& obj_list_;
  std::vector<BaseObjInfo*>::iterator itr_;
};

using FindBaseObj = FindObjInfo<BaseObjInfo>;
using FindBaseObjPtr = FindObjInfo<BaseObjInfo*>;
using FindDObjInfo = FindObjInfo<DObjInfo>;

}  // namespace

class ObjectData::Impl {
 public:
  Impl(ObjectData* self,
       const DObjPath& obj_path,
       const std::string& type,
       ObjectData* parent,
       Session* owner,
       bool is_flattened,
       bool init_directory,
       bool is_actual,
       bool enable_sorting);
  Impl(ObjectData* self,
       const FsPath& dir_path,
       const DObjPath& obj_path,
       const std::string& type,
       ObjectData* parent,
       Session* owner);
  ~Impl();

  bool HasKey(const std::string& key) const;
  DValue Get(const std::string& key, const DValue& default_value) const;
  DValue Get(const std::string& key) const;
  void Put(const std::string& key, const DValue& value);
  void RemoveKey(const std::string& key);
  bool IsLocalKey(const std::string& key) const;
  DObjPath WhereIsKey(const std::string& key) const;
  std::vector<std::string> Keys(bool local_only) const;

  bool HasAttr(const std::string& key) const;
  std::string Attr(const std::string& key) const;
  std::map<std::string, std::string> Attrs() const;
  void SetTemporaryAttr(const std::string& key, const std::string& value);
  void SetAllAttrsToBeSaved();
  void SetAttr(const std::string& key, const std::string& value);
  void RemoveAttr(const std::string& key);
  bool IsTemporaryAttr(const std::string& key) const;
  bool HasPersistentAttr(const std::string& key) const;

  std::string Type() const;
  std::deque<std::string> TypeChain() const;
  FsPath DirPath() const;
  DObjPath Path() const;

  bool IsActual() const;
  void SetIsActual(bool state);
  bool HasChild(const std::string& name) const;
  bool HasActualChild(const std::string& name) const;
  bool IsActualChild(const std::string& name) const;
  bool IsChildOpened(const std::string& name) const;
  DObjInfo ChildInfo(const std::string& name) const;
  std::vector<DObjInfo> Children() const;
  size_t ChildCount() const;
  bool IsFlattened() const;
  bool IsChildFlat(const std::string& name) const;
  void SetChildFlat(const std::string& name, bool flag_only = false);
  void UnsetChildFlat(const std::string& name);
  DObjectSp OpenChild(const std::string& name, OpenMode mode) const;
  DObjectSp CreateChild(const std::string& name,
                        const std::string& type,
                        bool is_flattened);
  DObjectSp Parent() const;
  DObjectSp GetObjectByPath(const DObjPath& obj_path, OpenMode mode);
  void AddChildInfo(const DObjInfo& child_info);
  void DeleteChild(const std::string& name);

  void AcquireWriteLock();
  void ReleaseWriteLock();

  void InitDirPath(const FsPath& dir_path);

  void SetDirty(bool dirty);
  bool IsDirty() const;
  bool IsEditable() const;

  void AddBase(const DObjectSp& base);
  std::vector<DObjectSp> Bases() const;
  void RemoveBase(const DObjectSp& base);

  void AddBaseFromParent(const DObjectSp& base);
  void AddBaseToChildren(const DObjectSp& base);
  void RemoveBaseFromChildren(const DObjPath& base_path);
  std::vector<DObjectSp> BasesFromParent() const;
  void RemoveBaseFromParent(const DObjectSp& base);
  bool HasObjectInBasesFromParent(const DObjPath& path) const;

  std::vector<DObjectSp> EffectiveBases() const;

  void UpdateEffectiveBaseList();
  void InstanciateBases() const;
  void InstanciateBases(std::vector<BaseObjInfo>& base_info_list) const;

  boost::signals2::connection AddListener(
      const ObjectListenerFunc& listener, ListenerCallPoint call_point);
  void DisableSignal();
  void EnableSignal();

  void Load();
  void Save(bool recurse);
  void RefreshChildren();
  void SortChildren();
  ObjectData* FindTop() const;

  CommandStackSp EnableCommandStack(bool enable);
  ObjectData* FindAncestorWithCommandStack() const;
  CommandStackSp GetCommandStack() const;
  CommandExecuterSp Executer() const;

  void IncRef();
  void DecRef(bool by_editable_ref);

  static DataSp Create(const std::string& name,
                       const std::string& type,
                       ObjectData* parent,
                       Session* owner);
  static DataSp Open(const std::string& name,
                     const std::string& type,
                     const FsPath& dir_path,
                     ObjectData* parent,
                     Session* owner);
  std::string DataFileName() const;
  FsPath DataFilePath() const;
  FsPath LockFilePath() const;

  void ExecUpdateValue(const std::string& key,
                       const DValue& new_value,
                       const DValue& prev_value);
  void ExecRemoveValue(const std::string& key, const DValue& prev_value);
  void ExecAddValue(const std::string& key, const DValue& new_value);
  void ExecDeleteChild(const std::string& name);
  void ExecAddBase(const DObjectSp& base);
  void ExecRemoveBase(const DObjectSp& base);

  Session* Owner() const { return owner_; }
  void EmitSignal(const Command& cmd, ListenerCallPoint call_point);

  void InitCompareFunc();

 private:
  void Save(const std::unique_ptr<DataIO>& data_io);
  bool InitDirPathImpl(const FsPath& dir_path);
  bool CreateEmpty(const FsPath& dir_path);
  void RemoveLockFile();
  void CreateLockFile();
  ObjectData* ChildData(const std::string& name,
                        bool open_if_not_opened) const;
  void RefreshActualChildren();
  void RefreshChildrenInBase() const;
  void ProcessBaseObjectUpdate(
      const Command& cmd, ListenerCallPoint call_point);

  CreateChildFunc GenCreateChildFunc(
      ObjectData* parent, std::vector<DataSp>* descendants);

  void SetupListener(const DObjectSp& base, BaseObjInfo& info) const;

  ObjectData* self_;
  ObjectData* parent_;
  DObjPath obj_path_;
  FsPath dir_path_;
  std::string type_;
  std::string data_file_name_;
  Session* owner_ = nullptr;

  DValueDict values_;
  DValueDict attrs_;
  DValueDict temp_attrs_;

  mutable std::vector<DObjInfo> actual_children_;
  mutable std::vector<DObjInfo> children_;

  mutable std::vector<BaseObjInfo> base_info_list_;
  mutable std::vector<BaseObjInfo> base_info_from_parent_list_;
  mutable std::vector<BaseObjInfo*> effective_base_info_list_;
  std::unordered_map<std::string, bool> child_flat_flags_;
  std::unique_ptr<boost::interprocess::file_lock> lock_file_;
  std::array<boost::signals2::signal<void (const Command&)>,
             static_cast<unsigned int>(ListenerCallPoint::kNumCallPoint)> sig_;
  CommandStackSp command_stack_;
  CommandExecuterSp default_command_executer_;
  FileFormat file_format_ = FileFormat::kJson;
  int editable_ref_count_ = 0;
  int ref_count_ = 0;
  bool dirty_ = false;
  bool signal_enabled_ = true;
  bool is_actual_ = false;
  DObjPath add_child_top_;
  DObjCompareFunc compare_func_;
  bool enable_sorting_ = true;
};

ObjectData::Impl::Impl(ObjectData* self,
                       const DObjPath& obj_path,
                       const std::string& type,
                       ObjectData* parent,
                       Session* owner,
                       bool is_flattened,
                       bool init_directory,
                       bool is_actual,
                       bool enable_sorting) :
    self_(self), parent_(parent), obj_path_(obj_path),
    type_(type), owner_(owner),
    default_command_executer_(new CommandExecuter(owner, self)),
    is_actual_(is_actual), enable_sorting_(enable_sorting) {
  InitCompareFunc();
  if (parent) {
    if (parent->IsFlattened())
      is_flattened = true;
    auto parent_dir_path = parent->DirPath();
    if (!is_flattened && !parent_dir_path.empty()) {
      if (init_directory)
        InitDirPath(parent_dir_path / obj_path_.LeafName());
      RefreshActualChildren();
    }
  }
}

ObjectData::Impl::Impl(ObjectData* self,
                       const FsPath& dir_path,
                       const DObjPath& obj_path,
                       const std::string& type,
                       ObjectData* parent,
                       Session* owner) :
    self_(self), parent_(parent), obj_path_(obj_path),
    dir_path_(dir_path), type_(type), owner_(owner),
    default_command_executer_(new CommandExecuter(owner, self)) {
  InitCompareFunc();
  data_file_name_ = DataIOFactory::DataFileName(type_, file_format_);
  RefreshActualChildren();
}
    
ObjectData::Impl::~Impl() {
  RemoveLockFile();
  for (auto& base_info : effective_base_info_list_)
    for (auto& connection : base_info->Connections())
      connection.disconnect();
}

struct HasKeyFuncInfo {
  HasKeyFuncInfo(const std::string& k, Session* o) : key(k), owner(o) {}
  std::string key;
  Session* owner;
  bool then(const BaseObjInfo&) const { return true; }
  bool else_() const { return false; }
};

bool ObjectData::Impl::HasKey(const std::string& key) const {
  if (IsLocalKey(key))
    return true;
  InstanciateBases();
  return ExecInObjWithKey<bool>(
      effective_base_info_list_, HasKeyFuncInfo(key, owner_));
}

struct GetWithDefaultFuncInfo {
  GetWithDefaultFuncInfo(
      const std::string& k, const DValue& def_value, Session* o)
      : key(k), default_value(def_value), owner(o) {}
  std::string key;
  DValue default_value;
  Session* owner;
  DValue then(const BaseObjInfo& info) const { return info.Obj()->Get(key); }
  DValue else_() const { return default_value; }
};

DValue ObjectData::Impl::Get(const std::string& key,
                             const DValue& default_value) const {
  auto itr = values_.find(key);
  if (itr != values_.cend())
    return itr->second;
  InstanciateBases();
  return ExecInObjWithKey<DValue>(
      effective_base_info_list_,
      GetWithDefaultFuncInfo(key, default_value, owner_));
}

struct GetFuncInfo {
  GetFuncInfo(const std::string& k, const DObjPath& p, Session* o)
      : key(k), path(p), owner(o) {}
  std::string key;
  DObjPath path;
  Session* owner;
  DValue then(const BaseObjInfo& info) const { return info.Obj()->Get(key); }
  DValue else_() const { THROW2(kErrNoKey, path.String(), key); }
};

DValue ObjectData::Impl::Get(const std::string& key) const {
  auto itr = values_.find(key);
  if (itr != values_.cend())
    return itr->second;
  InstanciateBases();
  return ExecInObjWithKey<DValue>(
      effective_base_info_list_, GetFuncInfo(key, Path(), owner_));
}

void ObjectData::Impl::Put(const std::string& key, const DValue& value) {
  auto itr = values_.find(key);
  auto edit_type = CommandType::kUnknown;
  auto prev_value = DValue(nil);
  if (itr == values_.cend()) {
    edit_type = CommandType::kAdd;
  } else if (itr->second != value) {
    prev_value = values_[key];
    if (prev_value != value)
      edit_type = CommandType::kUpdate;
  }
  if (edit_type != CommandType::kUnknown)
    Executer()->UpdateValue(edit_type, self_, key, value, prev_value);
}

void ObjectData::Impl::RemoveKey(const std::string& key) {
  auto itr = values_.find(key);
  if (itr == values_.cend())
    THROW2(kErrNoKey, Path().String(), key);
  Executer()->UpdateValue(CommandType::kDelete, self_, key, nil, itr->second);
}

bool ObjectData::Impl::IsLocalKey(const std::string& key) const {
  return values_.find(key) != values_.cend();
}

struct WhereFuncInfo {
  WhereFuncInfo(const std::string& k, const DObjPath& p, Session* o)
      : key(k), path(p), owner(o) {}
  std::string key;
  DObjPath path;
  Session* owner;
  DObjPath then(const BaseObjInfo& info) const { return info.Obj()->WhereIsKey(key); }
  DObjPath else_() const { THROW2(kErrNoKey, path.String(), key); }
};

DObjPath ObjectData::Impl::WhereIsKey(const std::string& key) const {
  if (IsLocalKey(key))
    return Path();
  InstanciateBases();
  return ExecInObjWithKey<DObjPath>(
      effective_base_info_list_, WhereFuncInfo(key, Path(), owner_));
}

std::vector<std::string> ObjectData::Impl::Keys(bool local_only) const {
  std::unordered_set<std::string> keys;
  for (auto& kv : values_)
    keys.insert(kv.first);
  if (!local_only)
    for (auto& base : EffectiveBases())
      for (auto& key : base->Keys(false))
        keys.insert(key);
  std::vector<std::string> result;
  result.reserve(keys.size());
  std::copy(keys.begin(), keys.end(), std::back_inserter(result));
  return result;
}

bool ObjectData::Impl::HasAttr(const std::string& key) const {
  for (auto& attrs : {attrs_, temp_attrs_}) {
    auto itr = attrs.find(key);
    if (itr != attrs.cend())
      return true;
  }
  return false;
}

std::string ObjectData::Impl::Attr(const std::string& key) const {
  for (auto& attrs : {temp_attrs_, attrs_}) {
    auto itr = attrs.find(key);
    if (itr != attrs.cend())
      return boost::get<std::string>(itr->second);
  }
  THROW2(kErrAttrDoesNotExist, Path().String(), key);
}

std::map<std::string, std::string> ObjectData::Impl::Attrs() const {
  std::map<std::string, std::string> result;
  for (auto& attrs : {attrs_, temp_attrs_})
    for (auto& kv : attrs)
      result[kv.first] = boost::get<std::string>(kv.second);
  return result;
}

void ObjectData::Impl::SetTemporaryAttr(const std::string& key,
                                        const std::string& value) {
  temp_attrs_[key] = value;
}

void ObjectData::Impl::SetAttr(const std::string& key, const std::string& value) {
  SetIsActual(true);
  SetDirty(true);
  attrs_[key] = value;
  auto itr = temp_attrs_.find(key);
  if (itr != temp_attrs_.end())
    temp_attrs_.erase(itr);
}

void ObjectData::Impl::SetAllAttrsToBeSaved() {
  bool set_flag = false;
  for (auto& kv : temp_attrs_) {
    if (!set_flag) {
      SetIsActual(true);
      SetDirty(true);
      set_flag = true;
    }
    attrs_[kv.first] = kv.second;
  }
  temp_attrs_.clear();
}

void ObjectData::Impl::RemoveAttr(const std::string& key) {
  if (HasPersistentAttr(key)) {
    SetDirty(true);
  }
  for (auto attrs : {&attrs_, &temp_attrs_}) {
    auto itr = attrs->find(key);
    if (itr != attrs->end())
      attrs->erase(key);
  }
}

bool ObjectData::Impl::IsTemporaryAttr(const std::string& key) const {
  return temp_attrs_.find(key) != temp_attrs_.cend();
}

bool ObjectData::Impl::HasPersistentAttr(const std::string& key) const {
  return attrs_.find(key) != attrs_.cend();
}

std::string ObjectData::Impl::Type() const {
  return type_;
}

std::deque<std::string> ObjectData::Impl::TypeChain() const {
  std::deque<std::string> chain;
  chain.push_front(Type());
  auto current = parent_;
  while (current) {
    chain.push_front(current->Type());
    current = current->impl_->parent_;
  }
  return chain;
}

FsPath ObjectData::Impl::DirPath() const {
  return dir_path_;
}

DObjPath ObjectData::Impl::Path() const {
  return obj_path_;
}

bool ObjectData::Impl::IsActual() const {
  return is_actual_;
}

void ObjectData::Impl::SetIsActual(bool state) {
  if (state != is_actual_ && parent_) {
    auto name = obj_path_.LeafName();
    auto res = FindDObjInfo(parent_->impl_->children_).ByName(name);
    res.Obj().SetIsActual(state);
    auto& actual_children = parent_->impl_->actual_children_;
    if (state) {
      if (!parent_->HasActualChild(name)) {
        actual_children.push_back(res.Obj());
        SortDObjInfoVector(actual_children, compare_func_, enable_sorting_);
      }
      is_actual_ = true;
      parent_->impl_->SetIsActual(true);
    } else {
      if (parent_->HasActualChild(name)) {
        actual_children.erase(FindDObjInfo(actual_children).ByName(name).Itr(),
                              actual_children.cend());
      }
      is_actual_ = false;
    }
  }
}

bool ObjectData::Impl::HasChild(const std::string& name) const {
  return FindDObjInfo(children_).ByName(name).Found();
}

bool ObjectData::Impl::HasActualChild(const std::string& name) const {
  return FindDObjInfo(actual_children_).ByName(name).Found();
}

bool ObjectData::Impl::IsActualChild(const std::string& name) const {
  auto res = FindDObjInfo(children_).ByName(name);
  if (!res.Found())
    THROW2(kErrChildNotExist, name, Path().String());
  return res.Obj().IsActual();
}

bool ObjectData::Impl::IsChildOpened(const std::string& name) const {
  auto res = FindDObjInfo(children_).ByName(name);
  if (!res.Found())
    return false;
  return owner_->IsOpened(res.Obj().Path());
}

DObjInfo ObjectData::Impl::ChildInfo(const std::string& name) const {
  auto res = FindDObjInfo(children_).ByName(name);
  if (!res.Found())
    return DObjInfo();
  return res.Obj();
}

std::vector<DObjInfo> ObjectData::Impl::Children() const {
  return children_;
}

size_t ObjectData::Impl::ChildCount() const {
  return children_.size();
}

bool ObjectData::Impl::IsFlattened() const {
  if (!parent_)
    return ObjectFactory::Instance().IsFlattenedObject(Type());
  return parent_->IsChildFlat(obj_path_.LeafName());
}

bool ObjectData::Impl::IsChildFlat(const std::string& name) const {
  auto itr = child_flat_flags_.find(name);
  if (itr == child_flat_flags_.cend())
    return false;
  return itr->second;
}

void ObjectData::Impl::SetChildFlat(const std::string& name, bool flag_only) {
  if (IsChildFlat(name))
    return;
  SetDirty(true);
  if (!flag_only) {
    try {
      auto child_data = ChildData(name, true);
      auto dir_initialized = !child_data->impl_->dir_path_.empty();
      if (dir_initialized)
        child_data->impl_->RemoveLockFile();
      for (auto grand_child_info : child_data->Children())
        child_data->SetChildFlat(grand_child_info.Name());
      if (dir_initialized) {
        CleanUpObjectDirectory(child_data->impl_->dir_path_);
        child_data->impl_->dir_path_.clear();
      }
    } catch (const DException&) {
    }
  }
  child_flat_flags_[name] = true;
}

void ObjectData::Impl::UnsetChildFlat(const std::string& name) {
  if (!HasActualChild(name))
    THROW2(kErrChildNotExist, name, Path().String());
  if (!IsChildFlat(name))
    return;
  SetDirty(true);
  auto child_data = ChildData(name, true);
  auto dir_initialized = !child_data->impl_->dir_path_.empty();
  if (dir_initialized)
    child_data->impl_->InitDirPathImpl(child_data->impl_->dir_path_);
  for (auto grand_child_info : child_data->Children())
    child_data->UnsetChildFlat(grand_child_info.Name());
  child_flat_flags_[name] = false;
}

DObjectSp ObjectData::Impl::OpenChild(const std::string& name, OpenMode mode) const {
  return owner_->OpenObject(obj_path_.ChildPath(name), mode);
}

DObjectSp ObjectData::Impl::CreateChild(
    const std::string& name, const std::string& type, bool is_flattened) {
  if (HasActualChild(name)) {
    auto child_path = obj_path_.ChildPath(name);
    auto child = owner_->OpenObject(child_path, OpenMode::kEditable);
    return child;
  }
  return Executer()->UpdateChildList(
      CommandType::kAdd, self_, name, type, is_flattened);
}

DObjectSp ObjectData::Impl::Parent() const {
  if (!parent_)
    return nullptr;
  return owner_->OpenObject(obj_path_.ParentPath(), OpenMode::kReadOnly);
}

DObjectSp ObjectData::Impl::GetObjectByPath(const DObjPath& obj_path,
                                            OpenMode mode) {
  return owner_->OpenObject(obj_path, mode);
}

void ObjectData::Impl::AddChildInfo(const DObjInfo& child_info) {
  if (HasActualChild(child_info.Name()))
    THROW2(kErrChildDataAlreadyExists, child_info.Name(), Path().String());
  actual_children_.emplace_back(child_info);
  children_.erase(std::remove_if(children_.begin(), children_.end(),
                                 [&child_info](auto& c) {
                                   return c.Name() == child_info.Name(); }),
                  children_.end());
  children_.emplace_back(child_info);
  SortChildren();
}

void ObjectData::Impl::DeleteChild(const std::string& name) {
  if (!HasActualChild(name))
    THROW2(kErrChildNotExist, name, Path().String());
  auto child_info = ChildInfo(name);
  Executer()->UpdateChildList(
      CommandType::kDelete, self_, name, child_info.Type(), IsChildFlat(name));
}

void ObjectData::Impl::AcquireWriteLock() {
  if (!dir_path_.empty()) {
    auto data_file_path = DataFilePath();
    if (!CurrentUser::Instance().IsWritable(data_file_path))
      THROW1(kErrNoWritePermission, data_file_path.string());
    CreateLockFile();
  }
  editable_ref_count_ ++;
}

void ObjectData::Impl::ReleaseWriteLock() {
  if (editable_ref_count_ == 0)
    return;
  editable_ref_count_ --;
  if (editable_ref_count_ == 0)
    RemoveLockFile();
}

void ObjectData::Impl::RemoveLockFile() {
  if (lock_file_) {
    lock_file_->unlock();
    fs::remove(LockFilePath());
    lock_file_.reset();
  }
}

void ObjectData::Impl::CreateLockFile() {
  auto lock_file_path = LockFilePath();
  if (!fs::exists(lock_file_path))
    if (!std::fstream(lock_file_path.c_str(), std::ios_base::out))
      THROW1(kErrNoWritePermission, lock_file_path.string());

  if (!lock_file_) 
    lock_file_ = std::make_unique<ipc::file_lock>(lock_file_path.c_str());
  
  if (!lock_file_->try_lock())
    THROW1(kErrFailedToGetFileLock, lock_file_path);
}

void ObjectData::Impl::InitDirPath(const FsPath& dir_path) {
  auto result = InitDirPathImpl(dir_path);
  if (!result) {
    CleanUpObjectDirectory(dir_path);
    THROW1(kErrFailedToCreateObjectDirectory, dir_path.string());
  }
}

bool ObjectData::Impl::InitDirPathImpl(const FsPath& dir_path) {
  if (parent_ && parent_->impl_->dir_path_.empty())
    THROW1(kErrParentDirectoryNotInitialized, Path().String());
  data_file_name_ = DataIOFactory::DataFileName(type_, file_format_);
  auto info = DataIOFactory::FindDataFileInfo(dir_path);
  if (info.IsValid()) {
    data_file_name_.clear();
    return false;
  }
  dir_path_ = dir_path;
  if (!fs::exists(dir_path)) {
    boost::system::error_code error;
    auto result = fs::create_directory(dir_path, error);
    if (!result || error) {
      dir_path_.clear();
      data_file_name_.clear();
      return false;
    }
  }
  if (!ObjectFactory::Instance().IsFlattenedObject(Type())) {
    for (auto& child_info : actual_children_) {
      if (IsChildFlat(child_info.Name()))
        continue;
      auto child = ChildData(child_info.Name(), false);
      if (!child)
        continue;
      if (!child->impl_->InitDirPathImpl(dir_path / child_info.Name())) {
        data_file_name_.clear();
        dir_path_.clear();
        return false;
      }
    }
  }
  if (!CreateEmpty(dir_path)) {
    data_file_name_.clear();
    dir_path_.clear();
    return false;
  }
  if (editable_ref_count_ > 0) {
    try {
      CreateLockFile();
    } catch (const DException&) {
      data_file_name_.clear();
      dir_path_.clear();
      return false;
    }
  }
  return true;
}

void ObjectData::Impl::SetDirty(bool dirty) {
  dirty_ = dirty;
  if (!dirty)
    // reset dirty flag recursively for flattened children
    for (auto& child_info : actual_children_)
      if (IsChildFlat(child_info.Name()))
        OpenChild(child_info.Name(), OpenMode::kEditable)->SetDirty(false);
}

bool ObjectData::Impl::IsDirty() const {
  if (!dirty_)
    // If not dirty, check dirty flag recursively from flattened children
    for (auto& child_info : actual_children_)
      if (IsChildFlat(child_info.Name()))
        if (OpenChild(child_info.Name(), OpenMode::kReadOnly)->IsDirty())
          return true;
  return dirty_;
}

void ObjectData::Impl::IncRef() {
  ref_count_ ++;
}

void ObjectData::Impl::DecRef(bool by_editable_ref) {
  ref_count_ --;
  if (by_editable_ref)
    ReleaseWriteLock();
}

bool ObjectData::Impl::IsEditable() const {
  return editable_ref_count_ != 0;
}

void ObjectData::Impl::AddBase(const DObjectSp& base) {
  if (base->IsExpired())
    THROW2(kErrExpiredObjectToBase, base->Path().String(), Path().String());

  if (FindBaseObj(base_info_list_).ByPath(base->Path()).Found())
    return;
  Executer()->UpdateBaseObjectList(CommandType::kAdd, self_, base);
}

std::vector<DObjectSp> ObjectData::Impl::Bases() const {
  std::vector<DObjectSp> objects;
  InstanciateBases();
  std::transform(
      base_info_list_.cbegin(), base_info_list_.cend(),
      std::back_inserter(objects), [](auto& info) { return info.Obj(); });
  return objects;
}

void ObjectData::Impl::RemoveBase(const DObjectSp& base) {
  InstanciateBases();
  if (!FindBaseObj(base_info_list_).ByPath(base->Path()).Found())
    THROW2(kErrNotBaseObject, base->Path().String(), Path().String());
  Executer()->UpdateBaseObjectList(CommandType::kDelete, self_, base);
}

void ObjectData::Impl::AddBaseFromParent(const DObjectSp& base) {
  if (base->IsExpired())
    THROW2(kErrExpiredObjectToBase, base->Path().String(), Path().String());

  if (FindBaseObj(base_info_from_parent_list_).ByPath(base->Path()).Found())
    return;

  auto prev_children = Children();
  Command cmd(CommandType::kAddBaseObject, Path(),
              "", nil, nil, base->Path(), "", prev_children);
  EmitSignal(cmd, ListenerCallPoint::kPre);
  BaseObjInfo base_info(base->Path(), base);
  if (!FindBaseObj(base_info_list_).ByPath(base->Path()).Found())
    SetupListener(base, base_info);
  base_info_from_parent_list_.push_back(base_info);
  UpdateEffectiveBaseList();
  RefreshChildrenInBase();
  AddBaseToChildren(base);
  SetDirty(true);
  EmitSignal(cmd, ListenerCallPoint::kPost);
  for (auto& child_info : actual_children_) {
    if (base->HasChild(child_info.Name())) {
      auto child_of_base = base->OpenChild(child_info.Name(), OpenMode::kReadOnly);
      OpenChild(child_info.Name(), OpenMode::kReadOnly)
          ->GetData()->AddBaseFromParent(child_of_base);
    }
  }
}

void ObjectData::Impl::AddBaseToChildren(const DObjectSp& base) {
  for (auto& child_info : Children()) {
    if (!owner_->IsOpened(child_info.Path()))
      continue;
    if (!base->HasChild(child_info.Name()))
      continue;
    auto child = OpenChild(child_info.Name(), OpenMode::kReadOnly);
    auto base_child = base->OpenChild(child_info.Name(), OpenMode::kReadOnly);
    child->GetData()->AddBaseFromParent(base_child);
  }
}

void ObjectData::Impl::RemoveBaseFromChildren(const DObjPath& base_path) {
  for (auto& child_info : Children()) {
    if (!owner_->IsOpened(child_info.Path()))
      continue;
    auto child_data = OpenChild(
        child_info.Name(), OpenMode::kReadOnly)->GetData();
    auto child_base_path = base_path.ChildPath(child_info.Name());
    child_data->impl_->RemoveBaseFromChildren(child_base_path);
    auto& base_list = child_data->impl_->base_info_from_parent_list_;
    auto res = FindBaseObj(base_list).ByPath(child_base_path);
    if (res.Found()) {
      for (auto& connection : res.Obj().Connections())
        connection.disconnect();
      base_list.erase(res.Itr());
      child_data->impl_->UpdateEffectiveBaseList();
      child_data->impl_->RefreshChildrenInBase();
    }
  }
}

std::vector<DObjectSp> ObjectData::Impl::BasesFromParent() const {
  std::vector<DObjectSp> objects;
  InstanciateBases();
  std::transform(
      base_info_from_parent_list_.cbegin(),
      base_info_from_parent_list_.cend(),
      std::back_inserter(objects), [](auto& info) { return info.Obj(); });
  return objects;
}

void ObjectData::Impl::RemoveBaseFromParent(const DObjectSp& base) {
  auto path = base->Path();
  auto res = FindBaseObj(base_info_from_parent_list_).ByPath(path);
  if (!res.Found())
    THROW2(kErrNotBaseObject, base->Path().String(), Path().String());

  auto prev_children = Children();
  Command cmd(CommandType::kRemoveBaseObject, Path(),
              "", nil, nil, path, "", prev_children);
  EmitSignal(cmd, ListenerCallPoint::kPre);
  for (auto& connection : res.Obj().Connections())
    connection.disconnect();
  base_info_from_parent_list_.erase(res.Itr(), base_info_from_parent_list_.end());
  UpdateEffectiveBaseList();
  RefreshChildrenInBase();
  EmitSignal(cmd, ListenerCallPoint::kPost);
}

bool ObjectData::Impl::HasObjectInBasesFromParent(const DObjPath& path) const {
  return FindBaseObj(base_info_from_parent_list_).ByPath(path).Found();
}

void ObjectData::Impl::UpdateEffectiveBaseList() {
  effective_base_info_list_.clear();
  for (auto& base_info : base_info_list_)
    effective_base_info_list_.push_back(&base_info);
  for (auto& base_info : base_info_from_parent_list_)
    if (!FindBaseObjPtr(effective_base_info_list_).ByPath(base_info.Path()).Found())
      effective_base_info_list_.push_back(&base_info);
}

void ObjectData::Impl::InstanciateBases() const {
  InstanciateBases(base_info_list_);
  InstanciateBases(base_info_from_parent_list_);
}

void ObjectData::Impl::InstanciateBases(
    std::vector<BaseObjInfo>& base_info_list) const {
  for (auto& base_info : base_info_list) {
    if (!base_info.Obj() || base_info.Obj()->IsExpired()) {
      auto base = owner_->OpenObject(base_info.Path(), OpenMode::kReadOnly);
      base_info.SetObj(base);
      SetupListener(base, base_info);
      for (auto& child_info : actual_children_) {
        if (!base->HasChild(child_info.Name()))
          continue;
        auto base_child_path = base->Path().ChildPath(child_info.Name());
        auto child_data = OpenChild(
            child_info.Name(), OpenMode::kReadOnly)->GetData();
        if (child_data->impl_->HasObjectInBasesFromParent(base_child_path))
          continue;
        auto child_of_base = base->OpenChild(
            child_info.Name(), OpenMode::kReadOnly);
        child_data->AddBaseFromParent(child_of_base);
      }
    }
  }
}

std::vector<DObjectSp> ObjectData::Impl::EffectiveBases() const {
  std::vector<DObjectSp> objects;
  InstanciateBases();
  std::transform(
      effective_base_info_list_.cbegin(), effective_base_info_list_.cend(),
      std::back_inserter(objects), [](auto& info) { return info->Obj(); });
  return objects;
}

boost::signals2::connection ObjectData::Impl::AddListener(
    const ObjectListenerFunc& listener, ListenerCallPoint call_point) {
  return sig_[static_cast<unsigned int>(call_point)].connect(listener);
}

void ObjectData::Impl::DisableSignal() {
  signal_enabled_ = false;
}

void ObjectData::Impl::EnableSignal() {
  signal_enabled_ = true;
}

void ObjectData::Impl::Save(bool recurse) {
  if (!IsActual())
    return;
  if (dir_path_.empty() && parent_ && !IsFlattened()) {
    auto cur_obj = FindTop();
    auto cur_dir = cur_obj->DirPath();
    auto remaining_path = obj_path_.Tail();
    while (!remaining_path.Empty()) {
      if (cur_obj->DirPath().empty())
        break;
      cur_obj = cur_obj->OpenChild(
          remaining_path.TopName(), OpenMode::kReadOnly)->GetData();
      cur_dir = cur_dir / remaining_path.TopName();
      remaining_path = remaining_path.Tail();
      if (cur_obj->IsFlattened())
        break;
      if (!cur_obj->DirPath().empty())
        continue;
      cur_obj->InitDirPath(cur_dir);
    }
  }
  auto io = DataIOFactory::Instance().Create(file_format_);
  auto file_path = DataFilePath();
  io->OpenForWrite(file_path);
  Save(io);
  io->CloseForWrite();
  if (recurse) {
    for (auto& child_info : actual_children_) {
      if (IsChildOpened(child_info.Name()) && !IsChildFlat(child_info.Name())) {
        auto child = OpenChild(
            child_info.Name(), OpenMode::kReadOnly);
        child->SetEditable();
        child->Save(recurse);
      }
    }
  }
}

void ObjectData::Impl::Save(const std::unique_ptr<DataIO>& io) {
  io->ToDataSection();
  io->WriteDict(values_);
  io->ToSectionUp();
  io->ToAttributeSection();
  StoreBaseToDict(base_info_list_, attrs_);
  io->WriteDict(attrs_);
  RemoveBaseFromDict(attrs_);
  io->ToSectionUp();
  io->ToChildrenSection();
  for (auto& child_info : children_) {
    if (!IsChildFlat(child_info.Name()) && !IsFlattened())
      continue;
    auto child = OpenChild(child_info.Name(), OpenMode::kReadOnly);
    child->PreSaveHook();
    if (child->IsActual()) {
      io->ToSection(child_info);
      auto child_data = ChildData(child_info.Name(), false);
      child_data->impl_->Save(io);
      io->ToSectionUp();
    }
  }
  io->ToSectionUp();
  SetDirty(false);
}

std::string ObjectData::Impl::DataFileName() const {
  return data_file_name_;
}

FsPath ObjectData::Impl::DataFilePath() const {
  if (parent_ && IsFlattened())
    THROW1(kErrObjectIsFlattened, Path().String());
  if (dir_path_.empty())
    THROW1(kErrObjectDirectoryNotInitialized, Path().String());
  return dir_path_ / DataFileName();
}

FsPath ObjectData::Impl::LockFilePath() const {
  fs::path path(DataFilePath());
  path += kLockFileSuffix;
  return path;
}

void ObjectData::Impl::ExecUpdateValue(const std::string& key,
                                       const DValue& new_value,
                                       const DValue& prev_value) {
  SetIsActual(true);
  Command cmd(CommandType::kValueUpdate, Path(),
              key, new_value, prev_value, DObjPath(), "", {});
  EmitSignal(cmd, ListenerCallPoint::kPre);
  values_[key] = new_value;
  SetDirty(true);
  EmitSignal(cmd, ListenerCallPoint::kPost);
}

void ObjectData::Impl::ExecRemoveValue(const std::string& key,
                                       const DValue& prev_value) {
  SetIsActual(true);
  Command cmd(CommandType::kValueDelete, Path(),
              key, nil, prev_value, DObjPath(), "", {});
  EmitSignal(cmd, ListenerCallPoint::kPre);
  values_.erase(key);
  SetDirty(true);
  EmitSignal(cmd, ListenerCallPoint::kPost);
}

void ObjectData::Impl::ExecAddValue(const std::string& key,
                                    const DValue& new_value) {
  SetIsActual(true);
  Command cmd(CommandType::kValueAdd, Path(),
              key, new_value, nil, DObjPath(), "", {});
  EmitSignal(cmd, ListenerCallPoint::kPre);
  values_[key] = new_value;
  SetDirty(true);
  EmitSignal(cmd, ListenerCallPoint::kPost);
}

void ObjectData::Impl::ExecDeleteChild(const std::string& name) {
  auto prev_children = children_;
  auto child_info = ChildInfo(name);
  auto is_flat_child = IsChildFlat(name);
  Command cmd(CommandType::kDeleteChild, Path(), "", nil, nil,
              child_info.Path(), child_info.Type(), prev_children);
  EmitSignal(cmd, ListenerCallPoint::kPre);
  Owner()->DeleteObjectImpl(child_info.Path());
  actual_children_.erase(
      std::remove_if(actual_children_.begin(), actual_children_.end(),
                     [&name](auto& c) { return c.Name() == name; }),
      actual_children_.end());
  RefreshChildrenInBase();
  if (is_flat_child)
    SetDirty(true);
  EmitSignal(cmd, ListenerCallPoint::kPost);
}

void ObjectData::Impl::ExecAddBase(const DObjectSp& base) {
  SetIsActual(true);
  auto prev_children = Children();
  Command cmd(CommandType::kAddBaseObject, Path(),
              "", nil, nil, base->Path(), "", prev_children);
  EmitSignal(cmd, ListenerCallPoint::kPre);
  BaseObjInfo base_info(base->Path(), base);
  SetupListener(base, base_info);
  base_info_list_.push_back(base_info);
  UpdateEffectiveBaseList();
  RefreshChildrenInBase();
  AddBaseToChildren(base);
  SetDirty(true);
  EmitSignal(cmd, ListenerCallPoint::kPost);
}

void ObjectData::Impl::ExecRemoveBase(const DObjectSp& base) {
  SetIsActual(true);
  auto prev_children = Children();
  auto base_path = base->Path();
  Command cmd(CommandType::kRemoveBaseObject, Path(),
              "", nil, nil, base_path, "", prev_children);
  EmitSignal(cmd, ListenerCallPoint::kPre);
  auto itr = std::remove_if(
      base_info_list_.begin(), base_info_list_.end(),
      [&base_path](auto& info) { return info.Obj()->Path() == base_path; });
  
  for (auto& connection : itr->Connections())
    connection.disconnect();
  base_info_list_.erase(itr, base_info_list_.end());
  UpdateEffectiveBaseList();
  RefreshChildrenInBase();
  RemoveBaseFromChildren(base_path);
  for (auto& prev_child : prev_children) {
    if (!HasChild(prev_child.Name())
        && owner_->IsOpened(Path().ChildPath(prev_child.Name())))
      owner_->DeleteObjectImpl(Path().ChildPath(prev_child.Name()));
  }
  SetDirty(true);
  EmitSignal(cmd, ListenerCallPoint::kPost);
}

void ObjectData::Impl::EmitSignal(
    const Command& cmd, ListenerCallPoint call_point) {
  // Need special handling for add child
  bool enable_signal_by_add_child =
      add_child_top_.Empty()
      || !cmd.ObjPath().IsDescendantOf(add_child_top_, true);
  if (cmd.Type() == CommandType::kAddChild
      || cmd.Type() == CommandType::kAddFlattenedChild) {
    if (call_point == ListenerCallPoint::kPre
        && add_child_top_.Empty()) {
      add_child_top_ = cmd.ObjPath();
    } else if (call_point == ListenerCallPoint::kPost
               && !add_child_top_.Empty()
               && cmd.ObjPath() == add_child_top_) {
      add_child_top_.Clear();
      enable_signal_by_add_child = true;
    }
  }
  bool is_to_emit = signal_enabled_ && enable_signal_by_add_child;

  if (is_to_emit) {
    sig_[static_cast<unsigned int>(call_point)](cmd);
    auto ancestor_cmd_stack_obj = FindAncestorWithCommandStack();
    if (ancestor_cmd_stack_obj)
      ancestor_cmd_stack_obj->impl_->EmitSignal(cmd, call_point);
  }
}

bool ObjectData::Impl::CreateEmpty(const FsPath& dir_path) {
  std::ofstream f((dir_path / DataFileName()).string());
  if (!f)
    return false;
  f << "{}";
  f.close();
  return true;
}

void ObjectData::Impl::Load() {
  auto io = DataIOFactory::Instance().Create(file_format_);
  DValueDict values;
  DValueDict attrs;
  std::vector<DataSp> descendants;
  auto f = GenCreateChildFunc(self_, &descendants);
  auto arg = std::make_shared<ReadDataArg>(&values, &attrs, f);
  io->Load(DataFilePath(), arg);
  std::swap(values_, values);
  std::swap(attrs_, attrs);
  RestoreBaseFromDict(attrs_, base_info_list_);
  UpdateEffectiveBaseList();
  std::vector<DObjPath> registered_paths;
  for (auto descendant : descendants) {
    try {
      owner_->RegisterObjectData(descendant);
      registered_paths.emplace_back(descendant->Path());
      descendant->impl_->is_actual_ = true;
    } catch (const DException&) {
      for (auto registered_path : registered_paths)
        owner_->PurgeObject(registered_path);
      throw;
    }
  }
  for (auto descendant : descendants) {
    descendant->impl_->enable_sorting_ = true;
    descendant->impl_->SortChildren();
  }
  if (effective_base_info_list_.size() > 0)
    RefreshChildrenInBase();
  is_actual_ = true;
  SetDirty(false);
}

void ObjectData::Impl::RefreshChildren() {
  RefreshActualChildren();
  RefreshChildrenInBase();
}

void ObjectData::Impl::SortChildren() {
  SortDObjInfoVector(children_, compare_func_, enable_sorting_);
}

void ObjectData::Impl::RefreshActualChildren() {
  if (dir_path_.empty())
    return;
  std::vector<DObjInfo> children;
  std::copy_if(actual_children_.cbegin(), actual_children_.cend(),
               std::back_inserter(children),
               [this] (auto& c) { return this->IsChildFlat(c.Name()); });
  actual_children_.erase(
      std::remove_if(
          actual_children_.begin(), actual_children_.end(),
          [this] (auto& c) { return !this->IsChildFlat(c.Name()); }),
      actual_children_.end());
  auto dirs = boost::make_iterator_range(
      fs::directory_iterator(dir_path_),
      fs::directory_iterator());
  for (auto itr : dirs) {
    if (fs::is_directory(itr)) {
      auto file_info = DataIOFactory::FindDataFileInfo(itr.path());
      if (!file_info.IsValid())
        continue;
      auto child_name = file_info.DirName();
      if (HasActualChild(child_name))
        continue;
      children.emplace_back(
          DObjInfo(obj_path_.ChildPath(child_name), file_info.Type()));
    }
  }
  SortDObjInfoVector(children, compare_func_, enable_sorting_);
  if (children == actual_children_)
    return;
  std::swap(actual_children_, children);
  children_.erase(
      std::remove_if(
          children_.begin(), children_.end(),
          [] (auto& c) { return c.IsActual(); }),
      children_.end());
  children_.insert(
      children_.begin(), actual_children_.begin(), actual_children_.end());
  SortChildren();
}

void ObjectData::Impl::RefreshChildrenInBase() const {
  std::unordered_set<std::string> names;
  for (auto& child : actual_children_)
    names.insert(child.Name());
  children_.clear();
  children_.insert(children_.begin(),
                   actual_children_.cbegin(), actual_children_.cend());
  InstanciateBases();
  for (auto& base_info : effective_base_info_list_) {
    for (auto base_child : base_info->Obj()->Children()) {
      if (names.find(base_child.Name()) != names.cend())
        continue;
      names.insert(base_child.Name());
      base_child.SetIsActual(false);
      base_child.SetPath(obj_path_.ChildPath(base_child.Name()));
      children_.emplace_back(base_child);
    }
  }
  SortDObjInfoVector(children_, compare_func_, enable_sorting_);
}

void ObjectData::Impl::ProcessBaseObjectUpdate(
    const Command& cmd, ListenerCallPoint call_point) {
  const unsigned int cmd_type = static_cast<unsigned int>(cmd.Type());
  const unsigned int children_update_mask =
      static_cast<unsigned int>(CommandType::kBaseObjectUpdateType)
      | static_cast<unsigned int>(CommandType::kChildListUpdateType);
  auto edit_type = static_cast<unsigned int>(cmd.Type()) & k_edit_type_mask;
  if (cmd_type & children_update_mask) {
    auto prev_children = children_;
    auto next_cmd_type = static_cast<CommandType>(
        edit_type | (cmd_type & k_command_group_mask));
    if (call_point == ListenerCallPoint::kPost) {
      auto target_name = cmd.TargetObjectName();
      auto target_path = cmd.TargetObjectPath();
      if (edit_type == static_cast<unsigned int>(CommandType::kAdd)) {
        RefreshChildrenInBase();
        if (IsChildOpened(target_name)) {
          auto base_child = owner_->OpenObject(target_path, OpenMode::kReadOnly);
          auto child = OpenChild(target_name, OpenMode::kReadOnly);
          child->GetData()->AddBaseFromParent(base_child);
        }
      } else if (edit_type == static_cast<unsigned int>(CommandType::kDelete)) {
        if (IsChildOpened(target_name)) {
          auto child_data = OpenChild(target_name, OpenMode::kReadOnly)->GetData();
          child_data->impl_->RemoveBaseFromChildren(target_path);
          auto& base_list = child_data->impl_->base_info_from_parent_list_;
          auto res = FindBaseObj(base_list).ByPath(target_path);
          for (auto& connection : res.Obj().Connections())
            connection.disconnect();
          base_list.erase(res.Itr());
          child_data->impl_->UpdateEffectiveBaseList();
          child_data->impl_->RefreshChildrenInBase();
        }
        UpdateEffectiveBaseList();
        RefreshChildrenInBase();
        if (!HasChild(target_name)
            && owner_->IsOpened(Path().ChildPath(target_name)))
          owner_->DeleteObjectImpl(Path().ChildPath(target_name));
      }
    }
    EmitSignal(Command(next_cmd_type, Path(), "", nil, nil,
                       cmd.ObjPath(), "", prev_children), call_point);
    return;
  }
  if (!IsLocalKey(cmd.Key()))
    EmitSignal(Command(cmd.Type(), Path(), cmd.Key(), cmd.NewValue(),
                       cmd.PrevValue(), DObjPath(), "", {}), call_point);
}

CommandStackSp ObjectData::Impl::EnableCommandStack(bool enable) {
  if (enable) {
    auto stack = GetCommandStack();
    if (stack)
      THROW1(kErrCommandStackAlreadyEnabled, stack->RootObjPath().String());
    command_stack_ = std::shared_ptr<CommandStack>(
        new CommandStack(owner_, self_));
  } else {
    command_stack_ = nullptr;
  }
  return command_stack_;
}

ObjectData* ObjectData::Impl::FindAncestorWithCommandStack() const {
  if (!parent_)
    return nullptr;
  if (parent_->impl_->command_stack_)
    return parent_;
  return parent_->impl_->FindAncestorWithCommandStack();
}

CommandStackSp ObjectData::Impl::GetCommandStack() const {
  if (command_stack_)
    return command_stack_;
  auto target_obj = FindAncestorWithCommandStack();
  if (target_obj)
    return target_obj->impl_->command_stack_;
  return nullptr;
}

CommandExecuterSp ObjectData::Impl::Executer() const {
  auto executer = GetCommandStack();
  if (executer)
    return executer;
  return default_command_executer_;
}

ObjectData* ObjectData::Impl::ChildData(const std::string& name,
                                        bool open_if_not_opened) const {
  if (!open_if_not_opened && !IsChildOpened(name))
    return nullptr;
  auto child = owner_->OpenObject(
      obj_path_.ChildPath(name), OpenMode::kReadOnly);
  if (!child)
    return nullptr;
  return child->GetData();
}

CreateChildFunc ObjectData::Impl::GenCreateChildFunc(
    ObjectData* parent, std::vector<DataSp>* descendants) {
  CreateChildFunc f = [=](const DObjInfo& obj_info) {
    auto child_path = parent->Path().ChildPath(obj_info.Name());
    DObjInfo child_info(child_path, obj_info.Type());
    auto data = std::shared_ptr<ObjectData>(new ObjectData(
        child_path, obj_info.Type(), parent, owner_, true, true, true, false));
    parent->SetChildFlat(obj_info.Name());
    descendants->emplace_back(data);
    auto arg = std::make_shared<ReadDataArg>(
        &(data->impl_->values_),
        &(data->impl_->attrs_),
        GenCreateChildFunc(data.get(), descendants));
    RestoreBaseFromDict(data->impl_->attrs_, data->impl_->base_info_list_);
    data->impl_->UpdateEffectiveBaseList();
    return arg;
  };
  return f;
}

ObjectData* ObjectData::Impl::FindTop() const {
  if (parent_)
    return parent_->impl_->FindTop();
  return self_;
}

void ObjectData::Impl::InitCompareFunc() {
  auto sort_compare_func =
      ObjectFactory::Instance().GetChildrenSortCompareFunc(type_);
  auto session = owner_;
  auto get_obj_func = [session](const DObjInfo& info) {
    return session->OpenObject(info.Path());
  };
  compare_func_ = [get_obj_func, sort_compare_func](auto& lhs, auto& rhs) {
    return sort_compare_func(get_obj_func, lhs, rhs);
  };
}

void ObjectData::Impl::SetupListener(
    const DObjectSp& base, BaseObjInfo& base_info) const {
  base_info.Connections().push_back(
      base->AddListener(
          [this](const Command& cmd) {
            const_cast<ObjectData::Impl*>(this)->ProcessBaseObjectUpdate(
                cmd, ListenerCallPoint::kPre); },
          ListenerCallPoint::kPost));
  base_info.Connections().push_back(
      base->AddListener(
          [this](const Command& cmd) {
            const_cast<ObjectData::Impl*>(this)->ProcessBaseObjectUpdate(
                cmd, ListenerCallPoint::kPost); },
          ListenerCallPoint::kPost));
}

ObjectData::ObjectData(const DObjPath& obj_path,
                       const std::string& type,
                       ObjectData* parent,
                       Session* owner,
                       bool is_flattened,
                       bool init_directory,
                       bool is_actual,
                       bool enable_sorting) :
    impl_(std::make_unique<Impl>(
        this, obj_path, type, parent, owner,
        is_flattened, init_directory, is_actual, enable_sorting)) {
}

ObjectData::ObjectData(const FsPath& dir_path,
                       const DObjPath& obj_path,
                       const std::string& type,
                       ObjectData* parent,
                       Session* owner) :
    impl_(std::make_unique<Impl>(
        this, dir_path, obj_path, type, parent, owner)) {
}

ObjectData::~ObjectData() = default;

bool ObjectData::HasKey(const std::string& key) const {
  return impl_->HasKey(key);
}

DValue ObjectData::Get(const std::string& key,
                       const DValue& default_value) const {
  return impl_->Get(key, default_value);
}

DValue ObjectData::Get(const std::string& key) const {
  return impl_->Get(key);
}

void ObjectData::Put(const std::string& key, const DValue& value) {
  impl_->Put(key, value);
}

void ObjectData::RemoveKey(const std::string& key) {
  impl_->RemoveKey(key);
}

bool ObjectData::IsLocalKey(const std::string& key) const {
  return impl_->IsLocalKey(key);
}

DObjPath ObjectData::WhereIsKey(const std::string& key) const {
  return impl_->WhereIsKey(key);
}

std::vector<std::string> ObjectData::Keys(bool local_only) const {
  return impl_->Keys(local_only);
}

bool ObjectData::HasAttr(const std::string& key) const {
  return impl_->HasAttr(key);
}

std::string ObjectData::Attr(const std::string& key) const {
  return impl_->Attr(key);
}

std::map<std::string, std::string> ObjectData::Attrs() const {
  return impl_->Attrs();
}

void ObjectData::SetTemporaryAttr(const std::string& key,
                                  const std::string& value) {
  impl_->SetTemporaryAttr(key, value);
}

void ObjectData::SetAttr(const std::string& key, const std::string& value) {
  impl_->SetAttr(key, value);
}

void ObjectData::SetAllAttrsToBeSaved() {
  impl_->SetAllAttrsToBeSaved();
}

void ObjectData::RemoveAttr(const std::string& key) {
  impl_->RemoveAttr(key);
}

bool ObjectData::IsTemporaryAttr(const std::string& key) const {
  return impl_->IsTemporaryAttr(key);
}

bool ObjectData::HasPersistentAttr(const std::string& key) const {
  return impl_->HasPersistentAttr(key);
}

std::string ObjectData::Type() const {
  return impl_->Type();
}

std::deque<std::string> ObjectData::TypeChain() const {
  return impl_->TypeChain();
}

FsPath ObjectData::DirPath() const {
  return impl_->DirPath();
}

DObjPath ObjectData::Path() const {
  return impl_->Path();
}

bool ObjectData::IsActual() const {
  return impl_->IsActual();
}

bool ObjectData::HasChild(const std::string& name) const {
  return impl_->HasChild(name);
}

bool ObjectData::HasActualChild(const std::string& name) const {
  return impl_->HasActualChild(name);
}

bool ObjectData::IsActualChild(const std::string& name) const {
  return impl_->IsActualChild(name);
}

bool ObjectData::IsChildOpened(const std::string& name) const {
  return impl_->IsChildOpened(name);
}

std::vector<DObjInfo> ObjectData::Children() const {
  return impl_->Children();
}

DObjInfo ObjectData::ChildInfo(const std::string& name) const {
  return impl_->ChildInfo(name);
}

size_t ObjectData::ChildCount() const {
  return impl_->ChildCount();
}

bool ObjectData::IsFlattened() const {
  return impl_->IsFlattened();
}

bool ObjectData::IsChildFlat(const std::string& name) const {
  return impl_->IsChildFlat(name);
}

void ObjectData::SetChildFlat(const std::string& name) {
  impl_->SetChildFlat(name);
}

void ObjectData::UnsetChildFlat(const std::string& name) {
  impl_->UnsetChildFlat(name);
}  

DObjectSp ObjectData::OpenChild(const std::string& name, OpenMode mode) const {
  return impl_->OpenChild(name, mode);
}

DObjectSp ObjectData::CreateChild(const std::string& name,
                                  const std::string& type,
                                  bool is_flattened) {
  return impl_->CreateChild(name, type, is_flattened);
}

DObjectSp ObjectData::Parent() const {
  return impl_->Parent();
}

uintptr_t ObjectData::ObjectId() const {
  return reinterpret_cast<uintptr_t>(this);
}

void ObjectData::AddChildInfo(const DObjInfo& child_info) {
  impl_->AddChildInfo(child_info);
}

void ObjectData::DeleteChild(const std::string& name) {
  impl_->DeleteChild(name);
}

void ObjectData::AcquireWriteLock() {
  impl_->AcquireWriteLock();
}

void ObjectData::ReleaseWriteLock() {
  impl_->ReleaseWriteLock();
}

void ObjectData::InitDirPath(const FsPath& dir_path) {
  return impl_->InitDirPath(dir_path);
}

void ObjectData::SetDirty(bool dirty) {
  impl_->SetDirty(dirty);
}

bool ObjectData::IsDirty() const {
  return impl_->IsDirty();
}

void ObjectData::IncRef() {
  impl_->IncRef();
}

void ObjectData::DecRef(bool by_editable_ref) {
  impl_->DecRef(by_editable_ref);
}

bool ObjectData::IsEditable() const {
  return impl_->IsEditable();
}

void ObjectData::AddBase(const DObjectSp& base) {
  impl_->AddBase(base);
}

std::vector<DObjectSp> ObjectData::Bases() const {
  return impl_->Bases();
}

void ObjectData::RemoveBase(const DObjectSp& base) {
  impl_->RemoveBase(base);
}

void ObjectData::AddBaseFromParent(const DObjectSp& base) {
  impl_->AddBaseFromParent(base);
}

std::vector<DObjectSp> ObjectData::BasesFromParent() const {
  return impl_->BasesFromParent();
}

std::vector<DObjectSp> ObjectData::EffectiveBases() const {
  return impl_->EffectiveBases();
}

boost::signals2::connection ObjectData::AddListener(
    const ObjectListenerFunc& listener,  ListenerCallPoint call_point) {
  return impl_->AddListener(listener, call_point);
}

void ObjectData::DisableSignal() {
  impl_->DisableSignal();
}

void ObjectData::EnableSignal() {
  impl_->EnableSignal();
}

CommandStackSp ObjectData::EnableCommandStack(bool enable) {
  return impl_->EnableCommandStack(enable);
}
 
CommandStackSp ObjectData::GetCommandStack() const {
  return impl_->GetCommandStack();
}

void ObjectData::Save(bool recurse) {
  impl_->Save(recurse);
}

void ObjectData::RefreshChildren() {
  impl_->RefreshChildren();
}

void ObjectData::SortChildren() {
  impl_->SortChildren();
}

std::string ObjectData::DataFileName() const {
  return impl_->DataFileName();
}

FsPath ObjectData::DataFilePath() const {
  return impl_->DataFilePath();
}

FsPath ObjectData::LockFilePath() const {
  return impl_->LockFilePath();
}

void ObjectData::ExecUpdateValue(const std::string& key,
                                 const DValue& new_value,
                                 const DValue& prev_value) {
  impl_->ExecUpdateValue(key, new_value, prev_value);
}

void ObjectData::ExecRemoveValue(const std::string& key,
                                 const DValue& prev_value) {
  impl_->ExecRemoveValue(key, prev_value);
}

void ObjectData::ExecAddValue(const std::string& key,
                              const DValue& new_value) {
  impl_->ExecAddValue(key, new_value);
}

DObjectSp ObjectData::ExecCreateChild(const std::string& name,
                                      const std::string& type,
                                      bool is_flattened) {
  impl_->SetIsActual(true);
  auto child_path = Path().ChildPath(name);
  auto prev_children = Children();
  Command cmd(CommandType::kAddChild, Path(),
              "", nil, nil, child_path, type, prev_children);
  impl_->EmitSignal(cmd, ListenerCallPoint::kPre);
  DObjectSp child;
  if (HasActualChild(name)) {
    child = impl_->Owner()->OpenObject(child_path, OpenMode::kEditable);
  } else {
    child = impl_->Owner()->CreateObjectImpl(child_path, type, is_flattened);
    if (is_flattened)
      SetDirty(true);
  }
  impl_->EmitSignal(cmd, ListenerCallPoint::kPost);
  return child;
}

void ObjectData::ExecDeleteChild(const std::string& name) {
  impl_->ExecDeleteChild(name);
}

void ObjectData::ExecAddBase(const DObjectSp& base) {
  impl_->ExecAddBase(base);
}

void ObjectData::ExecRemoveBase(const DObjectSp& base) {
  impl_->ExecRemoveBase(base);
}

DataSp ObjectData::Create(const DObjPath& obj_path,
                          const std::string& type,
                          ObjectData* parent,
                          Session* owner,
                          bool is_flattened,
                          bool init_directory,
                          bool is_actual) {
  auto data = std::shared_ptr<ObjectData>(
      new ObjectData(obj_path, type, parent, owner,
                     is_flattened, init_directory, is_actual));
  if (parent && (parent->IsFlattened() || is_flattened)) {
    parent->impl_->SetChildFlat(obj_path.LeafName(), true);
    parent->SetDirty(true);
  }

  return data;
}

DataSp ObjectData::Open(const DObjPath& obj_path,
                        const FsPath& dir_path,
                        ObjectData* parent,
                        Session* owner) {
  auto file_info = DataIOFactory::FindDataFileInfo(dir_path);
  if (!file_info.IsValid())
    THROW1(kErrNotObjectDirectory, dir_path.string());
  auto data = std::shared_ptr<ObjectData>(new ObjectData(
      dir_path, obj_path, file_info.Type(), parent, owner));
  return data;
}

void ObjectData::Load() {
  impl_->Load();
  impl_->SetIsActual(true);
}

ObjectData* ObjectData::GetDataAt(const DObjPath& obj_path) {
  return impl_->GetObjectByPath(obj_path, OpenMode::kReadOnly)->GetData();
}

SessionPtr ObjectData::GetSession() {
  return impl_->Owner()->shared_from_this();
}

DObjFileInfo ObjectData::GetFileInfo(const FsPath& path) {
  return DataIOFactory::GetDataFileInfo(path);
}

}  // namespace detail

}  // namespace core

}  // namespace dino
