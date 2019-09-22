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

struct BaseObjInfo {
  BaseObjInfo() = default;
  BaseObjInfo(const DObjPath& p,
              const DObjectSp& o)
      : path(p), obj(o) {}
  DObjPath path;
  DObjectSp obj;
  std::vector<boost::signals2::connection> connections;
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
    attrs[key] = base_info_list[idx].path.String();
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
    if (base_info->obj->HasKey(func_info.key))
      return func_info.then(*base_info);
  }
  return func_info.else_();
}

void SortDObjInfoVector(std::vector<DObjInfo>& obj_list) {
  std::sort(obj_list.begin(),
            obj_list.end(),
            [](const auto& a, const auto& b) { return a < b; });
}

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
       bool is_local);
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
  bool IsLocal(const std::string& key) const;
  DObjPath Where(const std::string& key) const;
  std::vector<std::string> Keys(bool local_only) const;

  std::string Type() const;
  FsPath DirPath() const;
  DObjPath Path() const;

  bool IsActual() const;
  void SetIsActual(bool state);
  bool HasChild(const std::string& name) const;
  bool HasLocalChild(const std::string& name) const;
  bool IsLocalChild(const std::string& name) const;
  bool IsChildOpened(const std::string& name) const;
  DObjInfo ChildInfo(const std::string& name) const;
  std::vector<DObjInfo> Children() const;
  size_t ChildCount() const;
  bool IsFlattened() const;
  bool IsChildFlat(const std::string& name) const;
  void SetChildFlat(const std::string& name, bool flag_only = false);
  void UnsetChildFlat(const std::string& name);
  DObjectSp GetChildObject(size_t index) const;
  DObjectSp GetChildObject(const std::string& name) const;
  DObjectSp OpenChildObject(const std::string& name) const;
  DObjectSp CreateChild(const std::string& name,
                        const std::string& type,
                        bool is_flattened);
  DObjectSp Parent() const;
  DObjectSp GetObject(const DObjPath& obj_path);
  void AddChildInfo(const DObjInfo& child_info);
  void DeleteChild(const std::string& name);

  void AcquireWriteLock();
  void ReleaseWriteLock();

  void InitDirPath(const FsPath& dir_path);

  void SetDirty(bool dirty);
  bool IsDirty() const;
  bool IsEditable() const;

  void AddBase(const DObjectSp& base);
  std::vector<DObjectSp> BaseObjects() const;
  void RemoveBase(const DObjectSp& base);

  void AddBaseFromParent(const DObjectSp& base);
  std::vector<DObjectSp> BaseObjectsFromParent() const;
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

 private:
  void Save(const std::unique_ptr<DataIO>& data_io);
  bool InitDirPathImpl(const FsPath& dir_path);
  bool CreateEmpty(const FsPath& dir_path);
  void RemoveLockFile();
  void CreateLockFile();
  ObjectData* ChildData(const std::string& name,
                        bool open_if_not_opened) const;
  void RefreshLocalChildren();
  void RefreshChildrenInBase() const;
  void ProcessBaseObjectUpdate(
      const Command& cmd, ListenerCallPoint call_point);

  CreateChildFunc GenCreateChildFunc(
      ObjectData* parent, std::vector<DataSp>* descendants);

  ObjectData* self_;
  ObjectData* parent_;
  DObjPath obj_path_;
  FsPath dir_path_;
  std::string type_;
  std::string data_file_name_;
  Session* owner_ = nullptr;

  DValueDict values_;
  DValueDict attrs_;

  std::vector<DObjInfo> local_children_;
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
};

ObjectData::Impl::Impl(ObjectData* self,
                       const DObjPath& obj_path,
                       const std::string& type,
                       ObjectData* parent,
                       Session* owner,
                       bool is_flattened,
                       bool init_directory,
                       bool is_local) :
    self_(self), parent_(parent), obj_path_(obj_path),
    type_(type), owner_(owner),
    default_command_executer_(new CommandExecuter(owner, self)),
    is_actual_(is_local) {
  if (parent) {
    parent->AddChildInfo(DObjInfo(obj_path, type, is_local));
    if (parent->IsFlattened())
      is_flattened = true;
    auto parent_dir_path = parent->DirPath();
    if (!is_flattened && !parent_dir_path.empty()) {
      if (init_directory)
        InitDirPath(parent_dir_path / obj_path_.LeafName());
      RefreshLocalChildren();
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
  data_file_name_ = DataIOFactory::DataFileName(type_, file_format_);
  RefreshLocalChildren();
}
    
ObjectData::Impl::~Impl() {
  RemoveLockFile();
  for (auto& base_info : effective_base_info_list_)
    for (auto& connection : base_info->connections)
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
  if (IsLocal(key))
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
  DValue then(const BaseObjInfo& info) const { return info.obj->Get(key); }
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
  DValue then(const BaseObjInfo& info) const { return info.obj->Get(key); }
  DValue else_() const {
    BOOST_THROW_EXCEPTION(
        ObjectDataException(kErrNoKey)
        << ExpInfo1(path.String()) << ExpInfo2(key)); }
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
    BOOST_THROW_EXCEPTION(
        ObjectDataException(kErrNoKey)
        << ExpInfo1(Path().String()) << ExpInfo2(key));
  Executer()->UpdateValue(CommandType::kDelete, self_, key, nil, itr->second);
}

bool ObjectData::Impl::IsLocal(const std::string& key) const {
  return values_.find(key) != values_.cend();
}

struct WhereFuncInfo {
  WhereFuncInfo(const std::string& k, const DObjPath& p, Session* o)
      : key(k), path(p), owner(o) {}
  std::string key;
  DObjPath path;
  Session* owner;
  DObjPath then(const BaseObjInfo& info) const { return info.obj->Where(key); }
  DObjPath else_() const {
    BOOST_THROW_EXCEPTION(
        ObjectDataException(kErrNoKey)
        << ExpInfo1(path.String()) << ExpInfo2(key));
  }    
};

DObjPath ObjectData::Impl::Where(const std::string& key) const {
  if (IsLocal(key))
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

std::string ObjectData::Impl::Type() const {
  return type_;
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
    auto itr = std::find_if(parent_->impl_->children_.begin(),
                            parent_->impl_->children_.end(),
                            [&name](auto& info) { return info.Name() == name; });
    itr->SetIsLocal(state);
    auto& local_children = parent_->impl_->local_children_;
    if (state) {
      if (!parent_->HasLocalChild(name)) {
        local_children.push_back(*itr);
        SortDObjInfoVector(local_children);
      }
      is_actual_ = true;
      parent_->impl_->SetIsActual(true);
    } else {
      if (parent_->HasLocalChild(name)) {
        local_children.erase(
            std::find_if(local_children.cbegin(),
                         local_children.cend(),
                         [&name] (auto& info) { return info.Name() == name; }),
            local_children.cend());
      }
      is_actual_ = false;
    }
  }
}

bool ObjectData::Impl::HasChild(const std::string& name) const {
  return std::find_if(
      children_.cbegin(),
      children_.cend(),
      [&](auto& c) { return c.Name() == name; }) != children_.cend();
}

bool ObjectData::Impl::HasLocalChild(const std::string& name) const {
  return std::find_if(
      local_children_.cbegin(),
      local_children_.cend(),
      [&](auto& c) { return c.Name() == name; }) != local_children_.cend();
}

bool ObjectData::Impl::IsLocalChild(const std::string& name) const {
  auto itr = std::find_if(
      children_.cbegin(),
      children_.cend(),
      [&](auto& c) { return c.Name() == name; });
  if (itr == children_.cend())
    BOOST_THROW_EXCEPTION(
        ObjectDataException(kErrChildNotExist)
        << ExpInfo1(name) << ExpInfo2(Path().String()));
  return itr->IsLocal();
}

bool ObjectData::Impl::IsChildOpened(const std::string& name) const {
  auto children = Children();
  auto itr = std::find_if(
      children.cbegin(),
      children.cend(),
      [&](auto& c) { return c.Name() == name; });
  if (itr == children.cend())
    return false;
  return owner_->IsOpened(itr->Path());
}

DObjInfo ObjectData::Impl::ChildInfo(const std::string& name) const {
  auto children = Children();
  auto itr = std::find_if(
      children.cbegin(),
      children.cend(),
      [&](auto& c) { return c.Name() == name; });
  if (itr == children.cend())
    return DObjInfo();
  return *itr;
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
  if (!HasLocalChild(name))
    BOOST_THROW_EXCEPTION(
        ObjectDataException(kErrChildNotExist)
        << ExpInfo1(name) << ExpInfo2(Path().String()));
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
  if (!HasLocalChild(name))
    BOOST_THROW_EXCEPTION(
        ObjectDataException(kErrChildNotExist)
        << ExpInfo1(name) << ExpInfo2(Path().String()));
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

DObjectSp ObjectData::Impl::GetChildObject(size_t index) const {
  if (index > children_.size())
    BOOST_THROW_EXCEPTION(
        ObjectDataException(kErrChildIndexOutOfRange)
        << ExpInfo1(index) << ExpInfo2(Path().String()));
  return owner_->GetObject(children_[index].Path());
}

DObjectSp ObjectData::Impl::GetChildObject(const std::string& name) const {
  return owner_->GetObject(obj_path_.ChildPath(name));
}

DObjectSp ObjectData::Impl::OpenChildObject(const std::string& name) const {
  return owner_->OpenObject(obj_path_.ChildPath(name));
}

DObjectSp ObjectData::Impl::CreateChild(
    const std::string& name, const std::string& type, bool is_flattened) {
  if (HasLocalChild(name)) {
    auto child_path = obj_path_.ChildPath(name);
    auto child = owner_->OpenObject(child_path);
    child->SetEditable();
    return child;
  }
  return Executer()->UpdateChildList(
      CommandType::kAdd, self_, name, type, is_flattened);
}

DObjectSp ObjectData::Impl::Parent() const {
  if (!parent_)
    return nullptr;
  return owner_->GetObject(obj_path_.ParentPath());
}

DObjectSp ObjectData::Impl::GetObject(const DObjPath& obj_path) {
  return owner_->GetObject(obj_path);
}

void ObjectData::Impl::AddChildInfo(const DObjInfo& child_info) {
  if (HasLocalChild(child_info.Name()))
    BOOST_THROW_EXCEPTION(
        ObjectDataException(kErrChildDataAlreadyExists)
        << ExpInfo1(child_info.Name()) << ExpInfo2(Path().String()));
  local_children_.emplace_back(child_info);
  children_.erase(std::remove_if(children_.begin(), children_.end(),
                                 [&child_info](auto& c) {
                                   return c.Name() == child_info.Name(); }),
                  children_.end());
  children_.emplace_back(child_info);
  SortDObjInfoVector(children_);
}

void ObjectData::Impl::DeleteChild(const std::string& name) {
  if (!HasLocalChild(name))
    BOOST_THROW_EXCEPTION(
        ObjectDataException(kErrChildNotExist)
        << ExpInfo1(name) << ExpInfo2(Path().String()));
  auto child_info = ChildInfo(name);
  Executer()->UpdateChildList(
      CommandType::kDelete, self_, name, child_info.Type(), IsChildFlat(name));
}

void ObjectData::Impl::AcquireWriteLock() {
  if (!dir_path_.empty()) {
    auto data_file_path = DataFilePath();
    if (!CurrentUser::Instance().IsWritable(data_file_path))
      BOOST_THROW_EXCEPTION(
          ObjectDataException(kErrNoWritePermission)
          << ExpInfo1(data_file_path.string()));
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
      BOOST_THROW_EXCEPTION(
          ObjectDataException(kErrNoWritePermission)
          << ExpInfo1(lock_file_path.string()));

  if (!lock_file_) 
    lock_file_ = std::make_unique<ipc::file_lock>(lock_file_path.c_str());
  
  if (!lock_file_->try_lock())
    BOOST_THROW_EXCEPTION(
        ObjectDataException(kErrFailedToGetFileLock)
        << ExpInfo1(lock_file_path));
}

void ObjectData::Impl::InitDirPath(const FsPath& dir_path) {
  auto result = InitDirPathImpl(dir_path);
  if (!result) {
    CleanUpObjectDirectory(dir_path);
    BOOST_THROW_EXCEPTION(
        ObjectDataException(kErrFailedToCreateObjectDirectory)
        << ExpInfo1(dir_path.string()));
  }
}

bool ObjectData::Impl::InitDirPathImpl(const FsPath& dir_path) {
  if (parent_ && parent_->impl_->dir_path_.empty())
    BOOST_THROW_EXCEPTION(
        ObjectDataException(kErrParentDirectoryNotInitialized)
        << ExpInfo1(Path().String()));
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
    for (auto& child_info : local_children_) {
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
    for (auto& child_info : local_children_)
      if (IsChildFlat(child_info.Name()))
        GetChildObject(child_info.Name())->SetDirty(false);
}

bool ObjectData::Impl::IsDirty() const {
  if (!dirty_)
    // If not dirty, check dirty flag recursively from flattened children
    for (auto& child_info : local_children_)
      if (IsChildFlat(child_info.Name()))
        if (GetChildObject(child_info.Name())->IsDirty())
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
    BOOST_THROW_EXCEPTION(
        ObjectDataException(kErrExpiredObjectToBase)
        << ExpInfo1(base->Path().String()) << ExpInfo2(Path().String()));

  auto itr = std::find_if(base_info_list_.cbegin(), base_info_list_.cend(),
                          [&base](auto b) { return b.path == base->Path(); });
  if (itr != base_info_list_.cend())
    return;
  Executer()->UpdateBaseObjectList(CommandType::kAdd, self_, base);
}

std::vector<DObjectSp> ObjectData::Impl::BaseObjects() const {
  std::vector<DObjectSp> objects;
  InstanciateBases();
  std::transform(
      base_info_list_.cbegin(), base_info_list_.cend(),
      std::back_inserter(objects), [](auto& info) { return info.obj; });
  return objects;
}

void ObjectData::Impl::RemoveBase(const DObjectSp& base) {
  InstanciateBases();
  auto path = base->Path();
  auto itr = std::find_if(
      base_info_list_.begin(), base_info_list_.end(),
      [&path](auto& info) { return info.obj->Path() == path; });
  if (itr == base_info_list_.end())
    BOOST_THROW_EXCEPTION(
        ObjectDataException(kErrNotBaseObject)
        << ExpInfo1(base->Path().String()) << ExpInfo2(Path().String()));
  Executer()->UpdateBaseObjectList(CommandType::kDelete, self_, base);
}

void ObjectData::Impl::AddBaseFromParent(const DObjectSp& base) {
  if (base->IsExpired())
    BOOST_THROW_EXCEPTION(
        ObjectDataException(kErrExpiredObjectToBase)
        << ExpInfo1(base->Path().String()) << ExpInfo2(Path().String()));

  auto itr = std::find_if(base_info_list_.cbegin(), base_info_list_.cend(),
                          [&base](auto b) { return b.path == base->Path(); });
  if (itr != base_info_list_.cend())
    return;

  auto prev_children = Children();
  Command cmd(CommandType::kAddBaseObject, Path(),
              "", nil, nil, base->Path(), "", prev_children);
  EmitSignal(cmd, ListenerCallPoint::kPre);
  BaseObjInfo base_info(base->Path(), base);
  base_info.connections.push_back(
      base->AddListener(
          [this](const Command& cmd) {
            ProcessBaseObjectUpdate(cmd, ListenerCallPoint::kPre); },
          ListenerCallPoint::kPost));
  base_info.connections.push_back(
      base->AddListener(
          [this](const Command& cmd) {
            ProcessBaseObjectUpdate(cmd, ListenerCallPoint::kPost); },
          ListenerCallPoint::kPost));
  base_info_from_parent_list_.push_back(base_info);
  UpdateEffectiveBaseList();
  RefreshChildrenInBase();
  SetDirty(true);
  EmitSignal(cmd, ListenerCallPoint::kPost);
  for (auto& child_info : local_children_) {
    if (base->HasChild(child_info.Name())) {
      auto child_of_base = base->OpenChildObject(child_info.Name());
      OpenChildObject(
          child_info.Name())->GetData()->AddBaseFromParent(child_of_base);
    }
  }
}

std::vector<DObjectSp> ObjectData::Impl::BaseObjectsFromParent() const {
  std::vector<DObjectSp> objects;
  InstanciateBases();
  std::transform(
      base_info_from_parent_list_.cbegin(),
      base_info_from_parent_list_.cend(),
      std::back_inserter(objects), [](auto& info) { return info.obj; });
  return objects;
}

void ObjectData::Impl::RemoveBaseFromParent(const DObjectSp& base) {
  auto path = base->Path();
  auto itr = std::find_if(
      base_info_from_parent_list_.begin(),
      base_info_from_parent_list_.end(),
      [&path](auto& info) { return info.obj->Path() == path; });
  if (itr == base_info_list_.end())
    BOOST_THROW_EXCEPTION(
        ObjectDataException(kErrNotBaseObject)
        << ExpInfo1(base->Path().String()) << ExpInfo2(Path().String()));

  auto prev_children = Children();
  Command cmd(CommandType::kRemoveBaseObject, Path(),
              "", nil, nil, path, "", prev_children);
  EmitSignal(cmd, ListenerCallPoint::kPre);
  for (auto& connection : itr->connections)
    connection.disconnect();
  base_info_from_parent_list_.erase(itr, base_info_from_parent_list_.end());
  UpdateEffectiveBaseList();
  RefreshChildrenInBase();
  EmitSignal(cmd, ListenerCallPoint::kPost);
}

bool ObjectData::Impl::HasObjectInBasesFromParent(const DObjPath& path) const {
  auto itr = std::find_if(base_info_from_parent_list_.cbegin(),
                          base_info_from_parent_list_.cend(),
                          [&path](auto& info) { return info.path == path; });
  return itr != base_info_from_parent_list_.cend();
}

void ObjectData::Impl::UpdateEffectiveBaseList() {
  effective_base_info_list_.clear();
  for (auto& base_info : base_info_list_)
    effective_base_info_list_.push_back(&base_info);
  for (auto& base_info : base_info_from_parent_list_)
    effective_base_info_list_.push_back(&base_info);
}

void ObjectData::Impl::InstanciateBases() const {
  InstanciateBases(base_info_list_);
  InstanciateBases(base_info_from_parent_list_);
}

void ObjectData::Impl::InstanciateBases(
    std::vector<BaseObjInfo>& base_info_list) const {
  for (auto& base_info : base_info_list) {
    if (!base_info.obj || base_info.obj->IsExpired()) {
      auto base = owner_->OpenObject(base_info.path);
      base_info.obj = base;
      base_info.connections.push_back(
          base->AddListener(
              [this](const Command& cmd) {
                const_cast<ObjectData::Impl*>(this)->ProcessBaseObjectUpdate(
                    cmd, ListenerCallPoint::kPre); },
              ListenerCallPoint::kPre));
      base_info.connections.push_back(
          base->AddListener(
              [this](const Command& cmd) {
                const_cast<ObjectData::Impl*>(this)->ProcessBaseObjectUpdate(
                    cmd, ListenerCallPoint::kPost); },
              ListenerCallPoint::kPost));
      for (auto& child_info : local_children_) {
        if (!base->HasChild(child_info.Name()))
          continue;
        auto base_child_path = base->Path().ChildPath(child_info.Name());
        auto child_data = OpenChildObject(child_info.Name())->GetData();
        if (child_data->impl_->HasObjectInBasesFromParent(base_child_path))
          continue;
        auto child_of_base = base->OpenChildObject(child_info.Name());
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
      std::back_inserter(objects), [](auto& info) { return info->obj; });
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
  if (!is_actual_ && EffectiveBases().size() > 0)
    BOOST_THROW_EXCEPTION(
        ObjectDataException(kErrObjectIsNotActual)
        << ExpInfo1(obj_path_.String()));
  if (dir_path_.empty() && parent_ && !IsFlattened()) {
    auto cur_obj = FindTop();
    auto cur_dir = cur_obj->DirPath();
    auto remaining_path = obj_path_.Tail();
    while (!remaining_path.Empty()) {
      if (cur_obj->DirPath().empty())
        break;
      cur_obj = cur_obj->GetChildObject(remaining_path.TopName())->GetData();
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
    for (auto& child_info : local_children_) {
      if (IsChildOpened(child_info.Name()) && !IsChildFlat(child_info.Name())) {
        auto child = GetChildObject(child_info.Name())->GetData();
        if (child->IsActual())
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
  for (auto& child : local_children_) {
    if (!IsChildFlat(child.Name()))
      continue;
    io->ToSection(child);
    auto child_data = ChildData(child.Name(), false);
    child_data->impl_->Save(io);
    io->ToSectionUp();
  }
  io->ToSectionUp();
  SetDirty(false);
}

std::string ObjectData::Impl::DataFileName() const {
  return data_file_name_;
}

FsPath ObjectData::Impl::DataFilePath() const {
  if (parent_ && IsFlattened())
    BOOST_THROW_EXCEPTION(
        ObjectDataException(kErrObjectIsFlattened)
        << ExpInfo1(Path().String()));
  if (dir_path_.empty())
    BOOST_THROW_EXCEPTION(
        ObjectDataException(kErrObjectDirectoryNotInitialized)
        << ExpInfo1(Path().String()));
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
  local_children_.erase(
      std::remove_if(local_children_.begin(), local_children_.end(),
                     [&name](auto& c) { return c.Name() == name; }),
      local_children_.end());
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
  base_info.connections.push_back(
      base->AddListener(
          [this](const Command& cmd) {
            ProcessBaseObjectUpdate(cmd, ListenerCallPoint::kPre); },
          ListenerCallPoint::kPre));
  base_info.connections.push_back(
      base->AddListener(
          [this](const Command& cmd) {
            ProcessBaseObjectUpdate(cmd, ListenerCallPoint::kPost); },
          ListenerCallPoint::kPost));

  base_info_list_.push_back(base_info);
  UpdateEffectiveBaseList();
  RefreshChildrenInBase();
  SetDirty(true);
  EmitSignal(cmd, ListenerCallPoint::kPost);
}

void ObjectData::Impl::ExecRemoveBase(const DObjectSp& base) {
  SetIsActual(true);
  auto prev_children = Children();
  auto path = base->Path();
  Command cmd(CommandType::kRemoveBaseObject, Path(),
              "", nil, nil, path, "", prev_children);
  EmitSignal(cmd, ListenerCallPoint::kPre);
  auto itr = std::remove_if(
      base_info_list_.begin(), base_info_list_.end(),
      [&path](auto& info) { return info.obj->Path() == path; });
  
  for (auto& connection : itr->connections)
    connection.disconnect();
  base_info_list_.erase(itr, base_info_list_.end());
  UpdateEffectiveBaseList();
  RefreshChildrenInBase();
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
  if (effective_base_info_list_.size() > 0)
    RefreshChildrenInBase();
  is_actual_ = true;
  SetDirty(false);
}

void ObjectData::Impl::RefreshChildren() {
  RefreshLocalChildren();
  RefreshChildrenInBase();
}

void ObjectData::Impl::RefreshLocalChildren() {
  if (dir_path_.empty())
    return;
  std::vector<DObjInfo> children;
  std::copy_if(local_children_.cbegin(), local_children_.cend(),
               std::back_inserter(children),
               [this] (auto& c) { return this->IsChildFlat(c.Name()); });
  local_children_.erase(
      std::remove_if(
          local_children_.begin(), local_children_.end(),
          [this] (auto& c) { return !this->IsChildFlat(c.Name()); }),
      local_children_.end());
  auto dirs = boost::make_iterator_range(
      fs::directory_iterator(dir_path_),
      fs::directory_iterator());
  for (auto itr : dirs) {
    if (fs::is_directory(itr)) {
      auto file_info = DataIOFactory::FindDataFileInfo(itr.path());
      if (!file_info.IsValid())
        continue;
      auto child_name = file_info.DirName();
      if (HasLocalChild(child_name))
        continue;
      children.emplace_back(
          DObjInfo(obj_path_.ChildPath(child_name), file_info.Type()));
    }
  }
  SortDObjInfoVector(children);
  if (children == local_children_)
    return;
  std::swap(local_children_, children);
  children_.erase(
      std::remove_if(
          children_.begin(), children_.end(),
          [] (auto& c) { return c.IsLocal(); }),
      children_.end());
  children_.insert(
      children_.begin(), local_children_.begin(), local_children_.end());
  SortDObjInfoVector(children_);
}

void ObjectData::Impl::RefreshChildrenInBase() const {
  std::unordered_set<std::string> names;
  for (auto& child : local_children_)
    names.insert(child.Name());
  children_.clear();
  children_.insert(children_.begin(),
                   local_children_.cbegin(), local_children_.cend());
  InstanciateBases();
  for (auto& base_info : effective_base_info_list_) {
    for (auto base_child : base_info->obj->Children()) {
      if (names.find(base_child.Name()) != names.cend())
        continue;
      names.insert(base_child.Name());
      base_child.SetIsLocal(false);
      base_child.SetPath(obj_path_.ChildPath(base_child.Name()));
      children_.emplace_back(base_child);
    }
  }
  SortDObjInfoVector(children_);
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
    if (call_point == ListenerCallPoint::kPost)
      RefreshChildrenInBase();
    EmitSignal(Command(next_cmd_type, Path(), "", nil, nil,
                       cmd.ObjPath(), "", prev_children), call_point);
    return;
  }
  if (!IsLocal(cmd.Key()))
    EmitSignal(Command(cmd.Type(), Path(), cmd.Key(), cmd.NewValue(),
                       cmd.PrevValue(), DObjPath(), "", {}), call_point);
}

CommandStackSp ObjectData::Impl::EnableCommandStack(bool enable) {
  if (enable) {
    auto stack = GetCommandStack();
    if (stack)
      BOOST_THROW_EXCEPTION(
          ObjectDataException(kErrCommandStackAlreadyEnabled)
          << ExpInfo1(stack->RootObjPath().String()));
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
  auto child = owner_->GetObject(obj_path_.ChildPath(name));
  if (!child && open_if_not_opened)
    child = owner_->OpenObject(obj_path_.ChildPath(name));
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
        child_path, obj_info.Type(), parent, owner_, true));
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

ObjectData::ObjectData(const DObjPath& obj_path,
                       const std::string& type,
                       ObjectData* parent,
                       Session* owner,
                       bool is_flattened,
                       bool init_directory,
                       bool is_local) :
    impl_(std::make_unique<Impl>(
        this, obj_path, type, parent, owner,
        is_flattened, init_directory, is_local)) {
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

bool ObjectData::IsLocal(const std::string& key) const {
  return impl_->IsLocal(key);
}

DObjPath ObjectData::Where(const std::string& key) const {
  return impl_->Where(key);
}

std::vector<std::string> ObjectData::Keys(bool local_only) const {
  return impl_->Keys(local_only);
}


std::string ObjectData::Type() const {
  return impl_->Type();
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

bool ObjectData::HasLocalChild(const std::string& name) const {
  return impl_->HasLocalChild(name);
}

bool ObjectData::IsLocalChild(const std::string& name) const {
  return impl_->IsLocalChild(name);
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

DObjectSp ObjectData::GetChildObject(size_t index) const {
  return impl_->GetChildObject(index);
}

DObjectSp ObjectData::GetChildObject(const std::string& name) const {
  return impl_->GetChildObject(name);
}

DObjectSp ObjectData::OpenChildObject(const std::string& name) const {
  return impl_->OpenChildObject(name);
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

std::vector<DObjectSp> ObjectData::BaseObjects() const {
  return impl_->BaseObjects();
}

void ObjectData::RemoveBase(const DObjectSp& base) {
  impl_->RemoveBase(base);
}

void ObjectData::AddBaseFromParent(const DObjectSp& base) {
  impl_->AddBaseFromParent(base);
}

std::vector<DObjectSp> ObjectData::BaseObjectsFromParent() const {
  return impl_->BaseObjectsFromParent();
}

void ObjectData::RemoveBaseFromParent(const DObjectSp& base) {
  impl_->RemoveBaseFromParent(base);
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
  if (HasLocalChild(name)) {
    child = impl_->Owner()->OpenObject(child_path);
    child->SetEditable();
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
                          bool is_local) {
  auto data = std::shared_ptr<ObjectData>(
      new ObjectData(obj_path, type, parent, owner,
                     is_flattened, init_directory, is_local));
  if (parent && parent->IsFlattened()) {
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
    BOOST_THROW_EXCEPTION(
        ObjectDataException(kErrNotObjectDirectory)
        << ExpInfo1(dir_path.string()));
  auto data = std::shared_ptr<ObjectData>(new ObjectData(
      dir_path, obj_path, file_info.Type(), parent, owner));
  return data;
}

void ObjectData::Load() {
  impl_->Load();
  impl_->SetIsActual(true);
}

ObjectData* ObjectData::GetDataAt(const DObjPath& obj_path) {
  return impl_->GetObject(obj_path)->GetData();
}

ConstSessionPtr ObjectData::GetSession() const {
  return impl_->Owner()->shared_from_this();
}

DObjFileInfo ObjectData::GetFileInfo(const FsPath& path) {
  return DataIOFactory::GetDataFileInfo(path);
}

}  // namespace detail

}  // namespace core

}  // namespace dino
