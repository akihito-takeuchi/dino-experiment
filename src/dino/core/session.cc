// Copyright (c) 2018 Akihito Takeuchi
// Distributed under the MIT License : http://opensource.org/licenses/MIT

#include "dino/core/session.h"

#include <stdexcept>
#include <unordered_map>
#include <boost/filesystem.hpp>

#include <fmt/format.h>

#include "dino/core/sessionexception.h"
#include "dino/core/dobject.h"
#include "dino/core/dexception.h"
#include "dino/core/objectfactory.h"
#include "dino/core/detail/objectdata.h"

namespace dino {

namespace core {

namespace fs = boost::filesystem;
class Session::Impl {
 public:
  class TopObjPathInfo {
   public:
    TopObjPathInfo(
        const std::string& name, const FsPath& path)
        : name_(name)  {
      SetPath(path);
    }
    const std::string Name() const {
      return name_;
    }
    const FsPath& Path() const {
      return abs_path_;
    }
    void SetPath(const FsPath& path) {
      if (!path.empty())
        abs_path_ = fs::absolute(path);
      else
        abs_path_ = path;
    }
   private:
    std::string name_;
    FsPath abs_path_;
  };

  using TopObjPathVector = std::vector<std::shared_ptr<TopObjPathInfo>>;

  Impl(Session* self) : self_(self) {}
  ~Impl() = default;

  void AddTopLevelObjectPath(const std::string& name, const FsPath& dir_path);
  void RemoveTopLevelObjectPath(const std::string& name);
  std::shared_ptr<TopObjPathInfo> FindTopObjPathInfo(
      const std::string& name) const;
  bool HasTopLevelObject(const std::string& name) const;
  bool HasObjectData(const DObjPath& obj_path) const;

  std::vector<std::string> TopObjectNames() const;
  DObjectSp CreateTopLevelObject(const std::string& name,
                                 const std::string& type);
  void InitTopLevelObjectPath(const std::string& name,
                              const FsPath& dir_path);
  DObjectSp CreateObjectImpl(const DObjPath& obj_path,
                             const std::string& type,
                             bool is_flattened);
  DObjectSp GetObjectById(uintptr_t object_id, OpenMode mode) const;
  void PreNewObjectCheck(const DObjPath& obj_path) const;
  void PreOpenObjectCheck(const DObjPath& obj_path,
                          const FsPath& dir_path = FsPath(),
                          bool need_top_dir_path = true) const;
  DObjectSp MakeObject(const DObjPath& obj_path, OpenMode mode) const;
  DObjectSp MakeDefaultObject(const DObjPath& obj_path) const;
  DObjectSp OpenTopLevelObject(const FsPath& dir_path,
                               const std::string& name,
                               OpenMode mode);
  void OpenDataAtPath(const DObjPath& path, const FsPath& top_dir);
  DObjectSp OpenObject(const DObjPath& obj_path, OpenMode mode);
  void ExecPreOpenHook(const DObjPath& obj_path, OpenMode mode);
  void DeleteObjectImpl(const DObjPath& obj_path, bool delete_files = true);
  void PurgeObject(const DObjPath& obj_path, bool check_existence = true);
  void SetPreOpenHook(const PreOpenHookFuncType& pre_open_hook);
  void RegisterObjectData(const detail::DataSp& data);
  FsPath WorkspaceFilePath() const;

 private:
  TopObjPathVector object_paths_;
  std::unordered_map<DObjPath,
                     detail::DataSp,
                     DObjPath::Hash> obj_data_map_;
  Session* self_;
  PreOpenHookFuncType pre_open_hook_;
};

void Session::Impl::AddTopLevelObjectPath(const std::string& name,
                                          const FsPath& dir_path) {
  auto obj_info = std::make_shared<TopObjPathInfo>(name, dir_path);
  object_paths_.push_back(obj_info);
}

void Session::Impl::RemoveTopLevelObjectPath(const std::string& name) {
  object_paths_.erase(
      std::remove_if(
          object_paths_.begin(), object_paths_.end(),
          [&name](auto& obj_info) { return obj_info->Name() == name; }),
      object_paths_.end());
}

std::shared_ptr<Session::Impl::TopObjPathInfo> Session::Impl::FindTopObjPathInfo(
    const std::string& name) const {
  auto itr = std::find_if(
      object_paths_.cbegin(),
      object_paths_.cend(),
      [&name](auto& obj_info) { return obj_info->Name() == name; });
  if (itr == object_paths_.cend())
    return nullptr;
  return *itr;
}

bool Session::Impl::HasTopLevelObject(const std::string& name) const {
  return FindTopObjPathInfo(name) != nullptr;
}

bool Session::Impl::HasObjectData(const DObjPath& obj_path) const {
  return obj_data_map_.find(obj_path) != obj_data_map_.cend();
}

void Session::Impl::PreNewObjectCheck(const DObjPath& obj_path) const {
  auto name_info = ExpInfo1(obj_path.String());
  if (!obj_path.IsValid())
    BOOST_THROW_EXCEPTION(SessionException(kErrObjectName) << name_info);

  if (HasObjectData(obj_path))
    BOOST_THROW_EXCEPTION(
        SessionException(kErrObjectDataAlreadyExists) << name_info);

  if (obj_path.IsTop()) {
    if (FindTopObjPathInfo(obj_path.TopName()))
      BOOST_THROW_EXCEPTION(
          SessionException(kErrObjectDataAlreadyExists) << name_info);
    return;
  }

  DObjectSp parent_obj;
  try {
    parent_obj = MakeDefaultObject(obj_path.ParentPath());
  } catch (const DException&) {
    BOOST_THROW_EXCEPTION(
        SessionException(kErrParentObjectNotOpened) << name_info);
  }
    
  if (parent_obj->HasActualChild(obj_path.LeafName()))
    BOOST_THROW_EXCEPTION(
        SessionException(kErrObjectAlreadyExists) << name_info);
}

void Session::Impl::PreOpenObjectCheck(
    const DObjPath& obj_path,
    const FsPath& dir_path,
    bool need_top_dir_path) const {
  if (obj_path.Empty())
    BOOST_THROW_EXCEPTION(SessionException(kErrObjectPathEmpty));
        
  if (!obj_path.IsValid())
    BOOST_THROW_EXCEPTION(
        SessionException(kErrObjectName)
        << ExpInfo1(obj_path.String()));

  if (!obj_path.IsTop()) {
    auto top_obj_path_info = FindTopObjPathInfo(obj_path.TopName());
    auto has_object_data = HasObjectData(obj_path.Top());
    if (!top_obj_path_info && !has_object_data) {
      if (!top_obj_path_info)
        BOOST_THROW_EXCEPTION(
            SessionException(kErrTopLevelObjectNotExist)
            << ExpInfo1(obj_path.String()));
      else
        BOOST_THROW_EXCEPTION(
            SessionException(kErrParentObjectNotOpened)
            << ExpInfo1(obj_path.String()));
    }
    if (!dir_path.empty())
      BOOST_THROW_EXCEPTION(SessionException(kErrDirPathForNonTop));
    if (need_top_dir_path
        && FindTopObjPathInfo(obj_path.TopName())->Path().empty())
      BOOST_THROW_EXCEPTION(
          SessionException(kErrTopLevelObjectNotInitialized)
          << ExpInfo1(obj_path.TopName()));
  } else {
    auto path_info = FindTopObjPathInfo(obj_path.TopName());
    if (!path_info)
      return;
    if (path_info->Path() != fs::absolute(dir_path))
      BOOST_THROW_EXCEPTION(
          SessionException(kErrObjectAlreadyExists)
          << ExpInfo1(obj_path.TopName()));
  }
}

std::vector<std::string> Session::Impl::TopObjectNames() const {
  std::vector<std::string> names;
  std::transform(object_paths_.begin(), object_paths_.end(),
                 std::back_inserter(names),
                 [](auto& p) { return p->Name(); });
  return names;
}

DObjectSp Session::Impl::CreateTopLevelObject(
    const std::string& name, const std::string& type) {
  PreNewObjectCheck(DObjPath(name));
  AddTopLevelObjectPath(name, FsPath());
  try {
    DObjPath obj_path(name);
    bool is_flattened = ObjectFactory::Instance().UpdateFlattenedFlag(
        type, false);

    RegisterObjectData(
        detail::ObjectData::Create(obj_path, type, nullptr,
                                   self_, is_flattened));
    DObjectSp obj(MakeObject(obj_path, OpenMode::kEditable));
    return obj;
  } catch (const DException& e) {
    RemoveTopLevelObjectPath(name);
    throw SessionException(e);
  }
}

void Session::Impl::InitTopLevelObjectPath(const std::string& name,
                                           const FsPath& dir_path) {
  auto name_info = ExpInfo1(name);
  if (!HasTopLevelObject(name))
    BOOST_THROW_EXCEPTION(
        SessionException(kErrObjectDoesNotExist) << name_info);

  auto path_info = FindTopObjPathInfo(name);
  if (!path_info->Path().empty())
    BOOST_THROW_EXCEPTION(
        SessionException(kErrTopLevelObjectAlreadyInitialized) << name_info);

  try {
    obj_data_map_[DObjPath(name)]->InitDirPath(fs::absolute(dir_path));
    path_info->SetPath(dir_path);
  } catch (const DException& e) {
    throw SessionException(e);
  }
}

DObjectSp Session::Impl::CreateObjectImpl(const DObjPath& obj_path,
                                          const std::string& type,
                                          bool is_flattened) {
  PreNewObjectCheck(obj_path);
  if (obj_path.IsTop())
    return CreateTopLevelObject(obj_path.LeafName(), type);

  is_flattened = ObjectFactory::Instance().UpdateFlattenedFlag(
      type, is_flattened);

  auto parent = obj_data_map_[obj_path.ParentPath()].get();
  if (parent->IsFlattened())
    is_flattened = true;

  RegisterObjectData(
      detail::ObjectData::Create(obj_path, type, parent, self_, is_flattened));
  auto obj = MakeObject(obj_path, OpenMode::kEditable);
  return obj;
}

DObjectSp Session::Impl::GetObjectById(uintptr_t object_id, OpenMode mode) const {
  detail::DataSp data;
  for (auto& path_data_pair : obj_data_map_) {
    if (reinterpret_cast<uintptr_t>(path_data_pair.second.get()) == object_id) {
      data = path_data_pair.second;
      break;
    }
  }
  if (!data)
    BOOST_THROW_EXCEPTION(
        SessionException(kErrObjectDataNotOpened)
        << ExpInfo1(fmt::format("OBJ_ID:{}", object_id)));
  auto obj = std::shared_ptr<DObject>(ObjectFactory::Instance().Create(data));
  if (mode == OpenMode::kEditable)
    obj->SetEditable();
  return obj;
}

DObjectSp Session::Impl::OpenTopLevelObject(const FsPath& dir_path,
                                            const std::string& name,
                                            OpenMode mode) {
  DObjPath obj_path(name);
  if (HasObjectData(obj_path))
    return MakeObject(obj_path, mode);

  PreOpenObjectCheck(obj_path, dir_path);

  try {
    auto abs_path = fs::absolute(dir_path);
    AddTopLevelObjectPath(name, abs_path);
    auto data = detail::ObjectData::Open(obj_path, abs_path, nullptr, self_);
    RegisterObjectData(data);
    data->Load();
    return MakeObject(obj_path, mode);
  } catch (const DException& e) {
    RemoveTopLevelObjectPath(name);
    throw SessionException(e);
  }
}

void Session::Impl::OpenDataAtPath(
    const DObjPath& path, const FsPath& top_dir) {
  auto parent_data = obj_data_map_[path.ParentPath()].get();
  try {
    auto data = detail::ObjectData::Open(
        path, top_dir / path.Tail().String(),
        parent_data, self_);
    RegisterObjectData(data);
    data->Load();
  } catch (const DException&) {
    auto name = path.LeafName();
    auto obj_info = parent_data->ChildInfo(name);
    if (!obj_info.IsValid())
      BOOST_THROW_EXCEPTION(
          SessionException(kErrObjectDoesNotExist)
          << ExpInfo1(path.String()));
    auto is_flattened = ObjectFactory::Instance().UpdateFlattenedFlag(
        obj_info.Type(), false);
    RegisterObjectData(detail::ObjectData::Create(
        path, obj_info.Type(), parent_data, self_, is_flattened, false, false));
    auto data = obj_data_map_[path];
    for (auto& base_of_parent : parent_data->EffectiveBases()) {
      if (base_of_parent->HasChild(name)) {
        auto base = base_of_parent->OpenChild(name, OpenMode::kReadOnly);
        data->AddBaseFromParent(base);
        data->SetDirty(false);
      }
    }
  }
}

DObjectSp Session::Impl::OpenObject(const DObjPath& obj_path, OpenMode mode) {
  if (HasObjectData(obj_path))
    return MakeObject(obj_path, mode);

  if (obj_path.IsTop()) {
    FsPath dir_path = FsPath(obj_path.TopName());
    auto path_info = FindTopObjPathInfo(obj_path.TopName());
    if (path_info)
      dir_path = path_info->Path();
    else
      BOOST_THROW_EXCEPTION(
          SessionException(kErrTopObjectDoesNotExist)
          << ExpInfo1(obj_path.TopName()));
    return OpenTopLevelObject(FsPath(obj_path.TopName()),
                              obj_path.TopName(), mode);
  }

  PreOpenObjectCheck(obj_path, FsPath(), false);

  auto top_dir = FindTopObjPathInfo(obj_path.TopName())->Path();
  try {
    DObjPath current_path;
    DObjPath remaining_path(obj_path);
    while (remaining_path.Depth() > 1) {
      current_path = current_path.ChildPath(remaining_path.TopName());
      remaining_path = remaining_path.Tail();
      if (!HasObjectData(current_path))
        OpenDataAtPath(current_path, top_dir);
    }
    current_path = current_path.ChildPath(remaining_path.TopName());
    OpenDataAtPath(current_path, top_dir);
    return MakeObject(current_path, mode);
  } catch (const DException& e) {
    throw SessionException(e);
  }
}

void Session::Impl::ExecPreOpenHook(const DObjPath& obj_path, OpenMode mode) {
  if (pre_open_hook_)
    pre_open_hook_(obj_path, mode);
}

void Session::Impl::DeleteObjectImpl(const DObjPath& obj_path,
                                     bool delete_files) {
  FsPath dir_to_remove;
  auto target_name = obj_path.LeafName();
  if (!obj_path.IsTop()) {
    auto parent_path = obj_path.ParentPath();
    auto parent = OpenObject(parent_path, OpenMode::kEditable);
    if (!parent->IsChildFlat(target_name) && !parent->DirPath().empty())
      dir_to_remove = parent->DirPath() / target_name;
  } else {
    auto path_info = FindTopObjPathInfo(target_name);
    if (path_info && delete_files)
      dir_to_remove = path_info->Path();
  }
  if (!dir_to_remove.empty()) {
    try {
      fs::remove_all(dir_to_remove);
    } catch (const fs::filesystem_error&) {
    }
  }
  PurgeObject(obj_path, false);
}

DObjectSp Session::Impl::MakeObject(const DObjPath& obj_path,
                                    OpenMode mode) const {
  if (!HasObjectData(obj_path))
    BOOST_THROW_EXCEPTION(
        SessionException(kErrObjectDataNotOpened)
        << ExpInfo1(obj_path.String()));
  auto data = obj_data_map_.find(obj_path)->second;
  auto obj = std::shared_ptr<DObject>(ObjectFactory::Instance().Create(data));
  if (mode == OpenMode::kEditable)
    obj->SetEditable();
  return obj;
}

DObjectSp Session::Impl::MakeDefaultObject(const DObjPath& obj_path) const {
  if (!HasObjectData(obj_path))
    BOOST_THROW_EXCEPTION(
        SessionException(kErrObjectDataNotOpened)
        << ExpInfo1(obj_path.String()));
  auto data = obj_data_map_.find(obj_path)->second;
  return std::make_shared<DObject>(data);
}

void Session::Impl::PurgeObject(const DObjPath& obj_path, bool check_existence) {
  if (!HasObjectData(obj_path)) {
    if (check_existence)
      BOOST_THROW_EXCEPTION(
          SessionException(kErrObjectDataNotOpened)
          << ExpInfo1(obj_path.String()));
  } else {
    for (auto& child : obj_data_map_[obj_path]->Children())
      PurgeObject(obj_path.ChildPath(child.Name()), false);
    obj_data_map_.erase(obj_path);
  }
  if (obj_path.IsTop())
    RemoveTopLevelObjectPath(obj_path.TopName());
}

void Session::Impl::SetPreOpenHook(const PreOpenHookFuncType& pre_open_hook) {
  pre_open_hook_ = pre_open_hook;
}

void Session::Impl::RegisterObjectData(const detail::DataSp& data) {
  auto obj_path = data->Path();
  if (HasObjectData(obj_path))
    BOOST_THROW_EXCEPTION(
        SessionException(kErrObjectDataAlreadyExists)
        << ExpInfo1(obj_path.String()));
  obj_data_map_[obj_path] = data;
  if (data->IsActual() && !obj_path.IsTop()) {
    auto parent = obj_data_map_[obj_path.ParentPath()];
    parent->AddChildInfo(DObjInfo(data->Path(), data->Type(), data->IsActual()));
  }
}

Session::Session() : impl_(std::make_unique<Impl>(this)) {
}

Session::~Session() = default;

std::vector<std::string> Session::TopObjectNames() const {
  return impl_->TopObjectNames();
}

DObjectSp Session::CreateTopLevelObject(const std::string& name,
                                        const std::string& type) {
  return impl_->CreateTopLevelObject(name, type);
}

DObjectSp Session::OpenTopLevelObject(const FsPath& dir_path,
                                      const std::string& type,
                                      OpenMode mode) {
  return impl_->OpenTopLevelObject(dir_path, type, mode);
}

void Session::InitTopLevelObjectPath(const std::string& name,
                                     const FsPath& dir_path) {
  impl_->InitTopLevelObjectPath(name, dir_path);
}

DObjectSp Session::CreateObject(const DObjPath& obj_path,
                                const std::string& type,
                                bool is_flattened) {
  if (obj_path.IsTop())
    return CreateObjectImpl(obj_path, type, is_flattened);

  auto parent = OpenObject(obj_path.ParentPath(), OpenMode::kEditable);
  return parent->CreateChild(obj_path.LeafName(), type, is_flattened);
}

DObjectSp Session::CreateObjectImpl(const DObjPath& obj_path,
                                    const std::string& type,
                                    bool is_flattened) {
  return impl_->CreateObjectImpl(obj_path, type, is_flattened);
}

DObjectSp Session::OpenObject(const DObjPath& obj_path, OpenMode mode) {
  impl_->ExecPreOpenHook(obj_path, mode);
  return impl_->OpenObject(obj_path, mode);
}

DObjectSp Session::GetObjectById(uintptr_t object_id, OpenMode mode) const {
  return impl_->GetObjectById(object_id, mode);
}

bool Session::IsOpened(const DObjPath& obj_path) const {
  return impl_->HasObjectData(obj_path);
}

void Session::DeleteObject(const DObjPath& obj_path) {
  if (obj_path.IsTop()) {
    DeleteObjectImpl(obj_path);
    return;
  }

  auto parent = OpenObject(obj_path.ParentPath(), OpenMode::kEditable);
  parent->DeleteChild(obj_path.LeafName());
}

void Session::RemoveTopLevelObject(const std::string& name, bool delete_files) {
  impl_->DeleteObjectImpl(name, delete_files);
}

void Session::DeleteObjectImpl(const DObjPath& obj_path) {
  impl_->DeleteObjectImpl(obj_path);
}

void Session::PurgeObject(const DObjPath& obj_path) {
  impl_->PurgeObject(obj_path);
}

void Session::SetPreOpenHook(const PreOpenHookFuncType& pre_open_hook) {
  impl_->SetPreOpenHook(pre_open_hook);
}

void Session::RegisterObjectData(const detail::DataSp& data) {
  impl_->RegisterObjectData(data);
}

SessionPtr Session::Create() {
  return std::unique_ptr<Session>(new Session());
}

}  // namespace core

}  // namespace dino
