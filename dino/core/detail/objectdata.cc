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

struct ListenType {
  static const unsigned int kChildListUpdate = 0x1 << 0; 
  static const unsigned int kValueUpdate     = 0x1 << 1; 
  static const unsigned int kAttrUpdate      = 0x1 << 2; 
};

struct BaseObjInfo {
  BaseObjInfo() = default;
  BaseObjInfo(const DObjPath& p, const DObjectSp& o) : path(p), obj(o) {}
  DObjPath path;
  DObjectSp obj;
};

struct ListenerObjInfo {
  ListenerObjInfo() = default;
  ListenerObjInfo(const DObjectSp& o, unsigned int f)
      : obj(o), listen_type_flags(f) {}
  DObjectSp obj;
  unsigned int listen_type_flags;
};

struct NotificationFlags {
  bool child_list_updated = false;
  void Clear() {
    child_list_updated = false;
  }
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
T ExecInObjWithKey(std::vector<BaseObjInfo>& base_info_list,
                   const F& func_info) {
  for (auto& base_info : base_info_list) {
    if (!base_info.obj || base_info.obj->IsExpired())
      base_info.obj = func_info.owner->OpenObject(base_info.path);
    if (base_info.obj->HasKey(func_info.key))
      return func_info.then(base_info);
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
       bool is_flattened);
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

  std::string Type() const;
  FsPath DirPath() const;
  DObjPath Path() const;

  bool HasChild(const std::string& name) const;
  bool HasLocalChild(const std::string& name) const;
  bool IsLocalChild(const std::string& name) const;
  std::vector<DObjInfo> Children() const;
  size_t ChildCount() const;
  bool IsFlattened() const;
  bool IsChildFlat(const std::string& name) const;
  void SetChildFlat(const std::string& name);
  void UnsetChildFlat(const std::string& name);
  DObjectSp GetChildObject(size_t index) const;
  DObjectSp GetChildObject(const std::string& name) const;
  DObjectSp CreateChild(const std::string& name,
                        const std::string& type,
                        bool is_flattened);
  void AddChildInfo(const DObjInfo& child_info);

  void AcquireWriteLock();
  void ReleaseWriteLock();

  void InitDirPath(const FsPath& dir_path);

  bool IsDirty() const;
  void SetDirty(bool dirty);
  bool IsEditable() const;

  void AddBase(const DObjectSp& base);
  std::vector<DObjectSp> BaseObjects() const;
  void RemoveBase(const DObjectSp& base);

  void Load();
  void Save();
  void RefreshChildren();

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
  void AddListener(ListenerObjInfo&& listener);
  void BecomeListner(const DObjectSp& subject, unsigned int flags);
  void Notify();
  void SetNeedBaseChildrenUpdate();

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
  std::vector<ListenerObjInfo> listeners_;
  mutable bool need_base_children_update_ = true;

  mutable std::vector<BaseObjInfo> base_info_list_;
  std::unordered_map<std::string, bool> child_flat_flags_;
  std::unique_ptr<boost::interprocess::file_lock> lock_file_;
  NotificationFlags notification_flags_;
  FileFormat file_format_ = FileFormat::kJson;
  int editable_ref_count_ = 0;
  int ref_count_ = 0;
  bool dirty_ = false;
};

ObjectData::Impl::Impl(ObjectData* self,
                       const DObjPath& obj_path,
                       const std::string& type,
                       ObjectData* parent,
                       Session* owner,
                       bool is_flattened) :
    self_(self), parent_(parent), obj_path_(obj_path),
    type_(type), owner_(owner) {
  if (parent) {
    parent->AddChildInfo(DObjInfo(obj_path, type));
    auto parent_dir_path = parent->DirPath();
    if (!is_flattened && !parent_dir_path.empty()) {
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
    dir_path_(dir_path), type_(type), owner_(owner) {
  data_file_name_ = DataIOFactory::DataFileName(type_, file_format_);
  RefreshLocalChildren();
}
    
ObjectData::Impl::~Impl() {
  RemoveLockFile();
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
  return ExecInObjWithKey<bool>(base_info_list_, HasKeyFuncInfo(key, owner_));
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
  return ExecInObjWithKey<DValue>(
      base_info_list_, GetWithDefaultFuncInfo(key, default_value, owner_));
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
  return ExecInObjWithKey<DValue>(
      base_info_list_, GetFuncInfo(key, Path(), owner_));
}

void ObjectData::Impl::Put(const std::string& key, const DValue& value) {
  auto itr = values_.find(key);
  if (itr == values_.cend() || itr->second != value)
    SetDirty(true);
  values_[key] = value;
}

void ObjectData::Impl::RemoveKey(const std::string& key) {
  auto itr = values_.find(key);
  if (itr == values_.cend())
    BOOST_THROW_EXCEPTION(
        ObjectDataException(kErrNoKey)
        << ExpInfo1(Path().String()) << ExpInfo2(key));
  SetDirty(true);
  values_.erase(itr);
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
  return ExecInObjWithKey<DObjPath>(
      base_info_list_, WhereFuncInfo(key, Path(), owner_));
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

bool ObjectData::Impl::HasChild(const std::string& name) const {
  RefreshChildrenInBase();
  return std::find_if(
      children_.cbegin(),
      children_.cend(),
      [&](auto& c) { return c.Name() == name; }) != children_.cend();
}

bool ObjectData::Impl::HasLocalChild(const std::string& name) const {
  RefreshChildrenInBase();
  return std::find_if(
      local_children_.cbegin(),
      local_children_.cend(),
      [&](auto& c) { return c.Name() == name; }) != local_children_.cend();
}

bool ObjectData::Impl::IsLocalChild(const std::string& name) const {
  RefreshChildrenInBase();
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

std::vector<DObjInfo> ObjectData::Impl::Children() const {
  RefreshChildrenInBase();
  return children_;
}

size_t ObjectData::Impl::ChildCount() const {
  RefreshChildrenInBase();
  return children_.size();
}

bool ObjectData::Impl::IsFlattened() const {
  if (!parent_)
    return false;
  return parent_->IsChildFlat(obj_path_.LeafName());
}

bool ObjectData::Impl::IsChildFlat(const std::string& name) const {
  auto itr = child_flat_flags_.find(name);
  if (itr == child_flat_flags_.cend())
    return false;
  return itr->second;
}

void ObjectData::Impl::SetChildFlat(const std::string& name) {
  if (!HasLocalChild(name))
    BOOST_THROW_EXCEPTION(
        ObjectDataException(kErrChildNotExist)
        << ExpInfo1(name) << ExpInfo2(Path().String()));
  if (IsChildFlat(name))
    return;
  SetDirty(true);
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
  RefreshChildrenInBase();
  if (index > children_.size())
    BOOST_THROW_EXCEPTION(
        ObjectDataException(kErrChildIndexOutOfRange)
        << ExpInfo1(index) << ExpInfo2(Path().String()));
  return owner_->GetObject(children_[index].Path());
}

DObjectSp ObjectData::Impl::GetChildObject(const std::string& name) const {
  return owner_->GetObject(obj_path_.ChildPath(name));
}

DObjectSp ObjectData::Impl::CreateChild(
    const std::string& name, const std::string& type, bool is_flattened) {
  return owner_->CreateObject(Path().ChildPath(name), type, is_flattened);
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
  notification_flags_.child_list_updated = true;
  SortDObjInfoVector(children_);
  Notify();
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
}

bool ObjectData::Impl::IsDirty() const {
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
  base_info_list_.emplace_back(
      BaseObjInfo(base->Path(), base));
  BecomeListner(base, ListenType::kChildListUpdate | ListenType::kValueUpdate);
}

std::vector<DObjectSp> ObjectData::Impl::BaseObjects() const {
  std::vector<DObjectSp> objects;
  std::transform(
      base_info_list_.cbegin(), base_info_list_.cend(),
      std::back_inserter(objects), [](auto& info) { return info.obj; });
  return objects;
}

void ObjectData::Impl::RemoveBase(const DObjectSp& base) {
  auto path = base->Path();
  auto base_count = base_info_list_.size();
  if (base_count > 0) {
    base_info_list_.erase(std::remove_if(
        base_info_list_.begin(), base_info_list_.end(),
        [&path](auto& info) { return info.obj->Path() == path; }));
    if (base_info_list_.size() == base_count)
      BOOST_THROW_EXCEPTION(
          ObjectDataException(kErrNotBaseObject)
          << ExpInfo1(base->Path().String()) << ExpInfo2(Path().String()));
  }
}

void ObjectData::Impl::Save() {
  auto io = DataIOFactory::Instance().Create(file_format_);
  auto file_path = DataFilePath();
  io->OpenForWrite(file_path);
  Save(io);
  io->CloseForWrite();
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
  if (IsFlattened())
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
  std::vector<DObjPath> registered_paths;
  for (auto descendant : descendants) {
    try {
      owner_->RegisterObjectData(descendant);
      descendant->SetDirty(false);
      registered_paths.emplace_back(descendant->Path());
    } catch (const DException&) {
      for (auto registered_path : registered_paths)
        owner_->PurgeObject(registered_path);
      throw;
    }
  }
  SetDirty(false);
}

void ObjectData::Impl::RefreshChildren() {
  RefreshLocalChildren();
  RefreshChildrenInBase();
  Notify();
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
  notification_flags_.child_list_updated = true;
}

void ObjectData::Impl::RefreshChildrenInBase() const {
  if (!need_base_children_update_)
    return;
  std::unordered_set<std::string> names;
  for (auto& child : local_children_)
    names.insert(child.Name());
  children_.clear();
  children_.insert(children_.begin(),
                   local_children_.cbegin(), local_children_.cend());
  for (auto& base_info : base_info_list_) {
    if (!base_info.obj || base_info.obj->IsExpired())
      base_info.obj = owner_->OpenObject(base_info.path);
    for (auto base_child : base_info.obj->Children()) {
      if (names.find(base_child.Name()) != names.cend())
        continue;
      names.insert(base_child.Name());
      base_child.SetIsLocal(false);
      children_.emplace_back(base_child);
    }
  }
  need_base_children_update_ = false;
}

void ObjectData::Impl::BecomeListner(const DObjectSp& subject, unsigned int flags) {
  subject->GetData()->impl_->AddListener(ListenerObjInfo(
      owner_->GetObject(Path()), flags));
}

void ObjectData::Impl::AddListener(ListenerObjInfo&& listener) {
  listeners_.emplace_back(listener);
}

void ObjectData::Impl::Notify() {
  // clean up listeners;
  std::unordered_set<DObjPath, DObjPath::Hash> invalid_listeners;
  for (auto& listener : listeners_)
    if (listener.obj->IsExpired())
      invalid_listeners.insert(listener.obj->Path());
  for (auto& invalid_obj_path : invalid_listeners)
    listeners_.erase(std::remove_if(
        listeners_.begin(), listeners_.end(),
        [&invalid_obj_path] (auto& l) {
          return l.obj->Path() == invalid_obj_path; }));

  // Notify as needed
  for (auto& listener : listeners_) {
    if ((listener.listen_type_flags & ListenType::kChildListUpdate)
        && (notification_flags_.child_list_updated))
      listener.obj->GetData()->impl_->SetNeedBaseChildrenUpdate();
  }
  notification_flags_.Clear();
}

void ObjectData::Impl::SetNeedBaseChildrenUpdate() {
  need_base_children_update_ = true;
  for (auto& listener : listeners_)
    if (listener.listen_type_flags & ListenType::kChildListUpdate)
      listener.obj->GetData()->impl_->SetNeedBaseChildrenUpdate();
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
    return arg;
  };
  return f;
}

ObjectData::ObjectData(const DObjPath& obj_path,
                       const std::string& type,
                       ObjectData* parent,
                       Session* owner,
                       bool is_flattened) :
    impl_(std::make_unique<Impl>(
        this, obj_path, type, parent, owner, is_flattened)) {
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

std::string ObjectData::Type() const {
  return impl_->Type();
}

FsPath ObjectData::DirPath() const {
  return impl_->DirPath();
}

DObjPath ObjectData::Path() const {
  return impl_->Path();
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

std::vector<DObjInfo> ObjectData::Children() const {
  return impl_->Children();
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

DObjectSp ObjectData::CreateChild(const std::string& name,
                                  const std::string& type,
                                  bool is_flattened) {
  return impl_->CreateChild(name, type, is_flattened);
}

void ObjectData::AddChildInfo(const DObjInfo& child_info) {
  impl_->AddChildInfo(child_info);
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

void ObjectData::Save() {
  impl_->Save();
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

DataSp ObjectData::Create(const DObjPath& obj_path,
                          const std::string& type,
                          ObjectData* parent,
                          Session* owner,
                          bool is_flattened) {
  auto data = std::shared_ptr<ObjectData>(
      new ObjectData(obj_path, type, parent, owner, is_flattened));
  if (parent && parent->IsFlattened()) {
    parent->SetChildFlat(obj_path.LeafName());
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
  data->impl_->Load();
  return data;
}

DObjFileInfo ObjectData::GetFileInfo(const FsPath& path) {
  return DataIOFactory::GetDataFileInfo(path);
}

}  // namespace detail

}  // namespace core

}  // namespace dino
