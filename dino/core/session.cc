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
    TopObjPathInfo(const std::string& name, const FsPath& path)
        : name(name), path(path) {}
    std::string name;
    FsPath path;
  };

  using TopObjPathVector = std::vector<std::shared_ptr<TopObjPathInfo>>;
  using AddPathFuncType =
      std::function<void (const std::string&, const FsPath&)>;

  Impl(Session* self) : self_(self) {}
  ~Impl() = default;

  void SetWorkspaceFilePath(const FsPath& file_path);
  void AddTopLevelObjectPath(const std::string& name,
                             const FsPath& dir_path,
                             bool is_local);
  void RemoveTopLevelObjectPath(const std::string& name);
  std::shared_ptr<TopObjPathInfo> FindTopObjPathInfo(
      const std::string& name) const;
  bool HasTopLevelObject(const std::string& name) const;
  bool HasObjectData(const DObjPath& obj_path) const;

  void ReadWorkspaceFile();
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
  DObjectSp GetObject(const DObjPath& obj_path) const;
  void PreNewObjectCheck(const DObjPath& obj_path) const;
  void PreOpenObjectCheck(const DObjPath& obj_path,
                          const FsPath& dir_path = FsPath()) const;
  DObjectSp MakeObject(const DObjPath& obj_path) const;
  DObjectSp OpenTopLevelObject(const FsPath& dir_path,
                               const std::string& name);
  DObjectSp OpenObject(const DObjPath& obj_path);
  void PurgeObject(const DObjPath& obj_path);
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
                                          bool is_local) {
  auto obj_info = std::make_shared<TopObjPathInfo>(name, dir_path);
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
  auto arr = doc.GetArray();
  auto itr = arr.Begin();
  auto end_itr = arr.End();
  for (; itr != end_itr; ++ itr) {
    if (!itr->IsObject())
      BOOST_THROW_EXCEPTION(
          SessionException(kErrWorkspaceFileError)
          << ExpInfo1(wsp_file_path.string())
          << ExpInfo2("The entries should be objects."));
    auto type = GetRjStringValue(*itr, "type");
    auto path = FsPath(GetRjStringValue(*itr, "path"));
    if (type == "object") {
      auto name = GetRjStringValue(*itr, "name", "object");
      try {
        RegisterObjectData(
            detail::ObjectData::Open(DObjPath(name), path, nullptr, self_));
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

void Session::Impl::ReadWorkspaceFile() {
  ReadWorkspaceFile_(
      wsp_file_path_, [this](const std::string& name, const FsPath& dir_path) {
        AddTopLevelObjectPath(name, dir_path, true); });
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

  if (obj_path.IsTop())
    return;

  DObjectSp parent_obj;
  try {
    parent_obj = GetObject(obj_path.ParentPath());
  } catch (const DException&) {
    BOOST_THROW_EXCEPTION(
        SessionException(kErrParentObjectNotOpened) << name_info);
  }
    
  if (parent_obj->HasLocalChild(obj_path.LeafName()))
    BOOST_THROW_EXCEPTION(
        SessionException(kErrObjectAlreadyExists) << name_info);
}

void Session::Impl::PreOpenObjectCheck(
    const DObjPath& obj_path, const FsPath& dir_path) const {
  if (obj_path.Empty())
    BOOST_THROW_EXCEPTION(SessionException(kErrObjectPathEmpty));
        
  if (!obj_path.IsValid())
    BOOST_THROW_EXCEPTION(
        SessionException(kErrObjectName)
        << ExpInfo1(obj_path.String()));

  if (!obj_path.IsTop()) {
    if (!HasObjectData(obj_path.Top()))
      BOOST_THROW_EXCEPTION(
          SessionException(kErrTopLevelObjectNotExist)
          << ExpInfo1(obj_path.String()));
    if (!dir_path.empty())
      BOOST_THROW_EXCEPTION(SessionException(kErrDirPathForNonTop));
    if (FindTopObjPathInfo(obj_path.TopName())->path.empty())
      BOOST_THROW_EXCEPTION(
          SessionException(kErrTopLevelObjectNotInitialized)
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
    RegisterObjectData(
        detail::ObjectData::Create(obj_path, type, nullptr, self_, false));
    DObjectSp obj(MakeObject(obj_path));
    obj->SetEditable();
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

  auto parent = obj_data_map_[obj_path.ParentPath()].get();
  RegisterObjectData(
      detail::ObjectData::Create(obj_path, type, parent, self_, is_flattened));
  auto obj = MakeObject(obj_path);
  obj->SetEditable();
  return obj;
}

DObjectSp Session::Impl::GetObject(const DObjPath& obj_path) const {
  auto obj = MakeObject(obj_path);
  return obj;
}

DObjectSp Session::Impl::OpenTopLevelObject(const FsPath& dir_path,
                                            const std::string& name) {
  DObjPath obj_path(name);
  if (HasObjectData(obj_path))
    return GetObject(obj_path);

  PreOpenObjectCheck(obj_path, dir_path);

  try {
    RegisterObjectData(
        detail::ObjectData::Open(obj_path, dir_path, nullptr, self_));
    AddTopLevelObjectPath(name, dir_path, true);
    return MakeObject(obj_path);
  } catch (const DException& e) {
    throw SessionException(e);
  }
}

DObjectSp Session::Impl::OpenObject(const DObjPath& obj_path) {
  if (obj_path.IsTop())
    return OpenTopLevelObject(FsPath(obj_path.TopName()),
                              obj_path.TopName());

  if (HasObjectData(obj_path))
    return GetObject(obj_path);

  PreOpenObjectCheck(obj_path);

  auto top_dir = FindTopObjPathInfo(obj_path.TopName())->path;
  detail::DataSp parent;
  try {
    DObjPath current_path;
    DObjPath remaining_path(obj_path);
    while (remaining_path.Depth() > 1) {
      current_path = current_path.ChildPath(remaining_path.TopName());
      remaining_path = remaining_path.Tail();
      if (!HasObjectData(current_path))
        RegisterObjectData(detail::ObjectData::Open(
            current_path, top_dir / current_path.Tail().String(),
            parent.get(), self_));
      parent = obj_data_map_[current_path];
    }
    current_path = current_path.ChildPath(remaining_path.TopName());
    RegisterObjectData(detail::ObjectData::Open(
        current_path, top_dir / current_path.Tail().String(),
        parent.get(), self_));
    return MakeObject(current_path);
  } catch (const DException& e) {
    throw SessionException(e);
  }
}

DObjectSp Session::Impl::MakeObject(const DObjPath& obj_path) const {
  if (!HasObjectData(obj_path))
    BOOST_THROW_EXCEPTION(
        SessionException(kErrObjectDataNotOpened)
        << ExpInfo1(obj_path.String()));
  auto data = obj_data_map_.find(obj_path)->second;
  return std::shared_ptr<DObject>(ObjectFactory::Instance().Create(data));
}

void Session::Impl::PurgeObject(const DObjPath& obj_path) {
  if (!HasObjectData(obj_path))
    BOOST_THROW_EXCEPTION(
        SessionException(kErrObjectDataNotOpened)
        << ExpInfo1(obj_path.String()));

  for (auto& child : obj_data_map_[obj_path]->Children())
    PurgeObject(obj_path.ChildPath(child.Name()));

  obj_data_map_.erase(obj_path);
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
                                      const std::string& type) {
  return impl_->OpenTopLevelObject(dir_path, type);
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

  auto parent = GetObject(obj_path.ParentPath());
  return parent->CreateChild(obj_path.LeafName(), type, is_flattened);
}

DObjectSp Session::CreateObjectImpl(const DObjPath& obj_path,
                                    const std::string& type,
                                    bool is_flattened) {
  return impl_->CreateObjectImpl(obj_path, type, is_flattened);
}

DObjectSp Session::OpenObject(const DObjPath& obj_path) {
  return impl_->OpenObject(obj_path);
}

DObjectSp Session::GetObject(const DObjPath& obj_path) const {
  return impl_->GetObject(obj_path);
}

FsPath Session::WorkspaceFilePath() const {
  return impl_->WorkspaceFilePath();
}

void Session::PurgeObject(const DObjPath& obj_path) {
  impl_->PurgeObject(obj_path);
}

void Session::Save() {
  impl_->Save();
}

bool Session::HasError() {
  return !impl_->HasError();
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
