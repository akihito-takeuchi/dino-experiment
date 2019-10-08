// Copyright (c) 2018 Akihito Takeuchi
// Distributed under the MIT License : http://opensource.org/licenses/MIT

#include "dino/core/session.h"

#include <stdexcept>
#include <unordered_map>
#include <boost/filesystem.hpp>

#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/filereadstream.h>
#include <rapidjson/filewritestream.h>
#include <rapidjson/error/en.h>
#include <fmt/format.h>

#include "dino/core/sessionexception.h"
#include "dino/core/dobject.h"
#include "dino/core/dexception.h"
#include "dino/core/objectfactory.h"
#include "dino/core/detail/objectdata.h"
#include "dino/core/detail/dataiofactory.h"
#include "dino/core/detail/jsonwritersupport.h"

namespace dino {

namespace core {

namespace fs = boost::filesystem;
namespace rj = rapidjson;

namespace {

const size_t kWspFileIOBufSize = 2048;

class WorkspaceFileReadError : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

std::string GetRjStringValue(const rj::Value& entry,
                           const char* key,
                           std::string entry_type) {
  if (entry_type.empty())
    entry_type = " ";
  else
    entry_type = " " + entry_type + " ";
  
  auto itr = entry.FindMember(key);
  if (itr == entry.MemberEnd())
    throw WorkspaceFileReadError(fmt::format("The{}entry must have '{}' key",
                                             entry_type, key));
  const auto& value = itr->value;
  if (!value.IsString())
    throw WorkspaceFileReadError(fmt::format("The value of '{}' must be string", key));
  return value.GetString();
}

std::string GetRjStringValue(const rj::Value& entry, const char* key) {
  return GetRjStringValue(entry, key, "");
}

}

class Session::Impl {
 public:
  struct TopObjPathInfo {
    TopObjPathInfo(
        const std::string& name, const FsPath& path, const FsPath& abs_path)
        : name(name), path(path), abs_path(abs_path) {}
    std::string name;
    FsPath path;
    FsPath abs_path;
  };

  using TopObjPathVector = std::vector<std::shared_ptr<TopObjPathInfo>>;
  using AddPathFuncType =
      std::function<void (const std::string&, const FsPath&)>;

  Impl(Session* self) : self_(self) {}
  ~Impl() = default;

  void SetWorkspaceFilePath(const FsPath& file_path);
  void AddTopLevelObjectPath(const std::string& name,
                             const FsPath& dir_path,
                             bool is_local,
                             FsPath abs_path = FsPath());
  void RemoveTopLevelObjectPath(const std::string& name);
  std::shared_ptr<TopObjPathInfo> FindTopObjPathInfo(
      const std::string& name) const;
  bool HasTopLevelObject(const std::string& name) const;
  bool HasObjectData(const DObjPath& obj_path) const;

  void ReadWorkspaceFile(bool add_as_local=true);
  void ReadWorkspaceFile_(
      const FsPath& wsp_file_path, AddPathFuncType add_path_func);
  rj::Document OpenJson(const FsPath& file_path);
  void CheckWorkspaceFileError(const FsPath& file_path,
                               const rj::Document& doc);
  void WorkspaceFileOpenCheck(const std::string& file_path);
  void Save();

  std::vector<std::string> TopObjectNames() const;
  DObjectSp CreateTopLevelObject(const std::string& name,
                                 const std::string& type);
  void InitTopLevelObjectPath(const std::string& name,
                              const FsPath& dir_path);
  DObjectSp CreateObjectImpl(const DObjPath& obj_path,
                             const std::string& type,
                             bool is_flattened);
  DObjectSp GetObject(const DObjPath& obj_path, OpenMode mode) const;
  DObjectSp GetObject(uintptr_t object_id, OpenMode mode) const;
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
  void DeleteObjectImpl(const DObjPath& obj_path, bool delete_files = true);
  void PurgeObject(const DObjPath& obj_path, bool check_existence = true);
  void RegisterObjectData(const detail::DataSp& data);
  bool HasError();
  std::string ErrorMessage() const;
  void ClearErrorMessage();
  void AddErrorMessage(const std::string& msg);
  FsPath WorkspaceFilePath() const;

 private:
  FsPath wsp_file_path_;
  TopObjPathVector object_paths_;
  TopObjPathVector local_object_paths_;
  std::unordered_map<DObjPath,
                     detail::DataSp,
                     DObjPath::Hash> obj_data_map_;
  std::string error_message_;
  Session* self_;
};

void Session::Impl::SetWorkspaceFilePath(const FsPath& file_path) {
  wsp_file_path_ = fs::absolute(file_path);
}

void Session::Impl::AddTopLevelObjectPath(const std::string& name,
                                          const FsPath& dir_path,
                                          bool is_local,
                                          FsPath abs_path) {
  if (abs_path.empty())
    abs_path = fs::absolute(dir_path);
  auto obj_info = std::make_shared<TopObjPathInfo>(name, dir_path, abs_path);
  object_paths_.emplace_back(obj_info);
  if (is_local)
    local_object_paths_.emplace_back(obj_info);
}

void Session::Impl::RemoveTopLevelObjectPath(const std::string& name) {
  std::vector<TopObjPathVector*> obj_path_lists{&object_paths_, &local_object_paths_};
  for (auto paths : obj_path_lists) {
    paths->erase(
        std::remove_if(
            paths->begin(), paths->end(),
            [&name](auto& obj_info) { return obj_info->name == name; }),
        paths->end());
  }
}

std::shared_ptr<Session::Impl::TopObjPathInfo> Session::Impl::FindTopObjPathInfo(
    const std::string& name) const {
  auto itr = std::find_if(
      object_paths_.cbegin(),
      object_paths_.cend(),
      [&name](auto& obj_info) { return obj_info->name == name; });
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

void Session::Impl::ReadWorkspaceFile_(
    const FsPath& wsp_file_path, AddPathFuncType add_path_func) {
  auto doc = OpenJson(wsp_file_path);
  CheckWorkspaceFileError(wsp_file_path, doc);
  for (auto& obj : doc.GetArray()) {
    if (!obj.IsObject())
      BOOST_THROW_EXCEPTION(
          SessionException(kErrWorkspaceFileError)
          << ExpInfo1(wsp_file_path.string())
          << ExpInfo2("The entries should be objects."));
    auto type = GetRjStringValue(obj, "type");
    auto path = FsPath(GetRjStringValue(obj, "path"));
    if (type == "object") {
      auto name = GetRjStringValue(obj, "name", "object");
      try {
        auto file_info = detail::DataIOFactory::FindDataFileInfo(path);
        if (!file_info.IsValid()) {
          AddErrorMessage(
              fmt::format(
                  "Object path '{}' is not an object directory. "
                  "Ignored object '{}'.", fs::absolute(path).string(), name));
          continue;
        }
        add_path_func(name, path);
      } catch (const DException& e) {
        AddErrorMessage(fmt::format(
            "Failed to open the object {} @ {}", name, path.string()));
        AddErrorMessage(e.GetErrorMessage());
      }
    } else if (type == "include") {
      ReadWorkspaceFile_(
          path, [this](const std::string& name, const FsPath& dir_path) {
            AddTopLevelObjectPath(name, dir_path, false); });
    } else {
      BOOST_THROW_EXCEPTION(
          SessionException(kErrWorkspaceFileError)
          << ExpInfo1(wsp_file_path.string())
          << ExpInfo2(fmt::format("Unknown entry type found -> {}", type)));
    }
  }
}

rj::Document Session::Impl::OpenJson(const FsPath& file_path) {
  std::FILE* f = std::fopen(file_path.string().c_str(), "rb");
  if (f == nullptr)
    BOOST_THROW_EXCEPTION(
        SessionException(kErrFailedToOpenWorkspaceFile)
        << ExpInfo1(file_path.string()) << ExpInfo2("reading"));

  char buf[kWspFileIOBufSize];
  rj::FileReadStream fs(f, buf, sizeof(buf));
  rj::Document doc;
  doc.ParseStream(fs);
  fclose(f);
  return doc;
}

void Session::Impl::CheckWorkspaceFileError(const FsPath& file_path,
                                            const rj::Document& doc) {
  if (doc.HasParseError()) {
    rj::ParseErrorCode code = doc.GetParseError();
    const char *msg = rj::GetParseError_En(code);
    BOOST_THROW_EXCEPTION(
        SessionException(kErrWorkspaceFileError)
        << ExpInfo1(file_path.string()) << ExpInfo2(msg));
  }
  if (!doc.IsArray())
    BOOST_THROW_EXCEPTION(
        SessionException(kErrWorkspaceFileError)
        << ExpInfo1(file_path.string())
        << ExpInfo2("The root of workspace file has to be array."));
}

void Session::Impl::ReadWorkspaceFile(bool add_as_local) {
  ReadWorkspaceFile_(
      wsp_file_path_,
      [this, add_as_local](const std::string& name, const FsPath& dir_path) {
        AddTopLevelObjectPath(name, dir_path, add_as_local); });
}

void Session::Impl::WorkspaceFileOpenCheck(const std::string& file_path) {
  std::FILE* test_f = std::fopen(wsp_file_path_.string().c_str(), "ab");
  if (!test_f)
    BOOST_THROW_EXCEPTION(
        SessionException(kErrFailedToOpenWorkspaceFile)
        << ExpInfo1(file_path) << ExpInfo2("writing"));
  std::fclose(test_f);
}

void Session::Impl::Save() {
  if (wsp_file_path_.empty())
    BOOST_THROW_EXCEPTION(SessionException(kErrWorkspaceFilePathNotSet));

  auto working_path = wsp_file_path_.string() + ".writing";
  WorkspaceFileOpenCheck(wsp_file_path_.string());
  WorkspaceFileOpenCheck(working_path);

  std::FILE* f = std::fopen(working_path.c_str(), "wb");
  char buf[kWspFileIOBufSize];
  rj::FileWriteStream fs(f, buf, sizeof(buf));
  rj::PrettyWriter<rj::FileWriteStream> writer(fs);
  writer.SetIndent(' ', 2);
  detail::WriteData<rj::PrettyWriter<rj::FileWriteStream>> data_writer(writer);
  writer.StartArray();
  for (auto info : local_object_paths_) {
    if (info->path.empty())
      continue;
    writer.StartObject();
    writer.Key("type");
    writer.String("object");
    writer.Key("name");
    writer.String(info->name.c_str());
    writer.Key("path");
    writer.String(info->path.string().c_str());
    writer.EndObject();
  }
  writer.EndArray();
  std::fclose(f);
  fs::rename(working_path, wsp_file_path_.string());
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
    
  if (parent_obj->HasLocalChild(obj_path.LeafName()))
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
    if (!FindTopObjPathInfo(obj_path.TopName()))
      BOOST_THROW_EXCEPTION(
          SessionException(kErrTopLevelObjectNotExist)
          << ExpInfo1(obj_path.String()));
    if (!HasObjectData(obj_path.Top()))
      BOOST_THROW_EXCEPTION(
          SessionException(kErrParentObjectNotOpened)
          << ExpInfo1(obj_path.String()));
    if (!dir_path.empty())
      BOOST_THROW_EXCEPTION(SessionException(kErrDirPathForNonTop));
    if (need_top_dir_path
        && FindTopObjPathInfo(obj_path.TopName())->path.empty())
      BOOST_THROW_EXCEPTION(
          SessionException(kErrTopLevelObjectNotInitialized)
          << ExpInfo1(obj_path.TopName()));
  } else {
    auto path_info = FindTopObjPathInfo(obj_path.TopName());
    if (!path_info)
      return;
    if (path_info->abs_path != fs::absolute(dir_path))
      BOOST_THROW_EXCEPTION(
          SessionException(kErrObjectAlreadyExists)
          << ExpInfo1(obj_path.TopName()));
  }
}

std::vector<std::string> Session::Impl::TopObjectNames() const {
  std::vector<std::string> names;
  std::transform(object_paths_.begin(), object_paths_.end(),
                 std::back_inserter(names),
                 [](auto& p) { return p->name; });
  return names;
}

DObjectSp Session::Impl::CreateTopLevelObject(
    const std::string& name, const std::string& type) {
  PreNewObjectCheck(DObjPath(name));
  try {
    DObjPath obj_path(name);
    bool is_flattened = ObjectFactory::Instance().UpdateFlattenedFlag(
        type, false);

    RegisterObjectData(
        detail::ObjectData::Create(obj_path, type, nullptr,
                                   self_, is_flattened));
    DObjectSp obj(MakeObject(obj_path, OpenMode::kEditable));
    AddTopLevelObjectPath(name, FsPath(), true);
    return obj;
  } catch (const DException& e) {
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
  if (!path_info->path.empty())
    BOOST_THROW_EXCEPTION(
        SessionException(kErrTopLevelObjectAlreadyInitialized) << name_info);

  try {
    obj_data_map_[DObjPath(name)]->InitDirPath(fs::absolute(dir_path));
    path_info->path = dir_path;
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

DObjectSp Session::Impl::GetObject(const DObjPath& obj_path,
                                   OpenMode mode) const {
  return MakeObject(obj_path, mode);
}

DObjectSp Session::Impl::GetObject(uintptr_t object_id, OpenMode mode) const {
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
    return GetObject(obj_path, mode);

  PreOpenObjectCheck(obj_path, dir_path);

  try {
    auto abs_path = fs::absolute(dir_path);
    auto data = detail::ObjectData::Open(obj_path, abs_path, nullptr, self_);
    RegisterObjectData(data);
    data->Load();
    AddTopLevelObjectPath(name, dir_path, true, abs_path);
    return MakeObject(obj_path, mode);
  } catch (const DException& e) {
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
      }
    }
  }
}

DObjectSp Session::Impl::OpenObject(const DObjPath& obj_path, OpenMode mode) {
  if (HasObjectData(obj_path))
    return GetObject(obj_path, mode);

  if (obj_path.IsTop()) {
    FsPath dir_path = FsPath(obj_path.TopName());
    auto path_info = FindTopObjPathInfo(obj_path.TopName());
    if (path_info)
      dir_path = path_info->abs_path;
    else
      BOOST_THROW_EXCEPTION(
          SessionException(kErrTopObjectDoesNotExist)
          << ExpInfo1(obj_path.TopName()));
    return OpenTopLevelObject(FsPath(obj_path.TopName()),
                              obj_path.TopName(), mode);
  }

  PreOpenObjectCheck(obj_path, FsPath(), false);

  auto top_dir = FindTopObjPathInfo(obj_path.TopName())->path;
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

void Session::Impl::DeleteObjectImpl(const DObjPath& obj_path,
                                     bool delete_files) {
  FsPath dir_to_remove;
  auto target_name = obj_path.LeafName();
  if (!obj_path.IsTop()) {
    auto parent_path = obj_path.ParentPath();
    auto parent = GetObject(parent_path, OpenMode::kEditable);
    if (!parent->IsChildFlat(target_name) && !parent->DirPath().empty())
      dir_to_remove = parent->DirPath() / target_name;
  } else {
    auto path_info = FindTopObjPathInfo(target_name);
    if (path_info && delete_files)
      dir_to_remove = path_info->path;
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
      PurgeObject(obj_path.ChildPath(child.Name()), check_existence);
    obj_data_map_.erase(obj_path);
  }
  if (obj_path.IsTop())
    RemoveTopLevelObjectPath(obj_path.TopName());
}

void Session::Impl::RegisterObjectData(const detail::DataSp& data) {
  auto obj_path = data->Path();
  if (HasObjectData(obj_path))
    BOOST_THROW_EXCEPTION(
        SessionException(kErrObjectDataAlreadyExists)
        << ExpInfo1(obj_path.String()));
  obj_data_map_[obj_path] = data;
}

void Session::Impl::AddErrorMessage(const std::string& msg) {
  error_message_ += msg;
  if (msg.back() != '\n')
    error_message_ += '\n';
}

bool Session::Impl::HasError() {
  return !error_message_.empty();
}

std::string Session::Impl::ErrorMessage() const {
  return error_message_;
}

void Session::Impl::ClearErrorMessage() {
  error_message_.clear();
}

FsPath Session::Impl::WorkspaceFilePath() const {
  return wsp_file_path_;
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

  auto parent = GetObject(obj_path.ParentPath(), OpenMode::kEditable);
  return parent->CreateChild(obj_path.LeafName(), type, is_flattened);
}

DObjectSp Session::CreateObjectImpl(const DObjPath& obj_path,
                                    const std::string& type,
                                    bool is_flattened) {
  return impl_->CreateObjectImpl(obj_path, type, is_flattened);
}

DObjectSp Session::OpenObject(const DObjPath& obj_path, OpenMode mode) {
  return impl_->OpenObject(obj_path, mode);
}

DObjectSp Session::GetObject(const DObjPath& obj_path, OpenMode mode) const {
  return impl_->GetObject(obj_path, mode);
}

DObjectSp Session::GetObject(uintptr_t object_id, OpenMode mode) const {
  return impl_->GetObject(object_id, mode);
}

bool Session::IsOpened(const DObjPath& obj_path) const {
  return impl_->HasObjectData(obj_path);
}

void Session::DeleteObject(const DObjPath& obj_path) {
  if (obj_path.IsTop()) {
    DeleteObjectImpl(obj_path);
    return;
  }

  auto parent = GetObject(obj_path.ParentPath(), OpenMode::kEditable);
  parent->DeleteChild(obj_path.LeafName());
}

void Session::RemoveTopLevelObject(const std::string& name, bool delete_files) {
  impl_->DeleteObjectImpl(name, delete_files);
}

void Session::DeleteObjectImpl(const DObjPath& obj_path) {
  impl_->DeleteObjectImpl(obj_path);
}

FsPath Session::WorkspaceFilePath() const {
  return impl_->WorkspaceFilePath();
}

void Session::ImportWorkspaceFile(const std::string& wsp_file_path) {
  ImportWorkspaceFile(FsPath(wsp_file_path));
}

void Session::ImportWorkspaceFile(const FsPath& wsp_file_path) {
  auto wsp_file_path_org = impl_->WorkspaceFilePath();
  impl_->SetWorkspaceFilePath(wsp_file_path);
  impl_->ReadWorkspaceFile(false);
  impl_->SetWorkspaceFilePath(wsp_file_path_org);
}

void Session::PurgeObject(const DObjPath& obj_path) {
  impl_->PurgeObject(obj_path);
}

void Session::Save() {
  impl_->Save();
}

bool Session::HasError() {
  return impl_->HasError();
}

std::string Session::ErrorMessage() const {
  return impl_->ErrorMessage();
}

void Session::ClearErrorMessage() {
  impl_->ClearErrorMessage();
}

void Session::RegisterObjectData(const detail::DataSp& data) {
  impl_->RegisterObjectData(data);
}

SessionPtr Session::Create() {
  return std::unique_ptr<Session>(new Session());
}

SessionPtr Session::Create(const std::string& wsp_file_path) {
  return Create(FsPath(wsp_file_path));
}

SessionPtr Session::Create(const FsPath& wsp_file_path) {
  auto session = std::unique_ptr<Session>(new Session());
  session->impl_->SetWorkspaceFilePath(wsp_file_path);

  if (!wsp_file_path.empty()) {
    if (fs::exists(wsp_file_path))
      BOOST_THROW_EXCEPTION(
          SessionException(kErrWorkspaceFileAlreadyExists)
          << ExpInfo1(wsp_file_path.string()));
    auto dir = ParentFsPath(wsp_file_path);
    if (!fs::exists(dir))
      if (!fs::create_directories(dir))
        BOOST_THROW_EXCEPTION(
            SessionException(kErrFailedToCreateDirectory)
            << ExpInfo1(dir.string()));
    session->Save();
  }
  return session;
}

SessionPtr Session::Open(const FsPath& wsp_file_path) {
  if (!fs::exists(wsp_file_path))
    BOOST_THROW_EXCEPTION(
        SessionException(kErrWorkspaceFileDoesNotExist)
        << ExpInfo1(wsp_file_path.string()));
  
  auto session = std::unique_ptr<Session>(new Session());
  session->impl_->SetWorkspaceFilePath(wsp_file_path);
  session->impl_->ReadWorkspaceFile();
  return session;
}

}  // namespace core

}  // namespace dino
