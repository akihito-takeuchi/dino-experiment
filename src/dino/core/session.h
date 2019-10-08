// Copyright (c) 2018 Akihito Takeuchi
// Distributed under the MIT License : http://opensource.org/licenses/MIT

#pragma once

#include <string>
#include <vector>

#include "dino/core/fspath.h"
#include "dino/core/dobjpath.h"
#include "dino/core/fwd.h"
#include "dino/core/filetypes.h"

namespace dino {

namespace core {

namespace detail {

class ObjectData;

}  // namespace detail

class Session : public std::enable_shared_from_this<Session> {
 public:
  Session(const Session&) = delete;
  Session& operator=(const Session&) = delete;
  ~Session();
  std::vector<std::string> TopObjectNames() const;
  DObjectSp CreateTopLevelObject(const std::string& name,
                                 const std::string& type);
  DObjectSp OpenTopLevelObject(const FsPath& dir_path,
                               const std::string& name,
                               OpenMode mode = OpenMode::kReadOnly);
  void InitTopLevelObjectPath(const std::string& name,
                              const FsPath& dir_path);
  DObjectSp CreateObject(const DObjPath& obj_info,
                         const std::string& type,
                         bool is_flattened = false);
  DObjectSp OpenObject(const DObjPath& obj_path,
                       OpenMode mode = OpenMode::kReadOnly);
  DObjectSp GetObject(const DObjPath& obj_path,
                      OpenMode mode = OpenMode::kReadOnly) const;
  DObjectSp GetObject(uintptr_t object_id,
                      OpenMode mode = OpenMode::kReadOnly) const;
  bool IsOpened(const DObjPath& obj_path) const;
  void DeleteObject(const DObjPath& obj_path);
  void RemoveTopLevelObject(const std::string& name, bool delete_files = false);
  FsPath WorkspaceFilePath() const;
  void ImportWorkspaceFile(const std::string& wsp_file_path);
  void ImportWorkspaceFile(const FsPath& wsp_file_path);
  void PurgeObject(const DObjPath& obj_path);
  void Save();
  bool HasError();
  std::string ErrorMessage() const;
  void ClearErrorMessage();

  static SessionPtr Create();
  static SessionPtr Create(const std::string& wsp_file_path);
  static SessionPtr Create(const FsPath& wsp_file_path);
  static SessionPtr Open(const FsPath& wsp_file_path);

 private:
  Session();
  DObjectSp CreateObjectImpl(const DObjPath& obj_info,
                             const std::string& type,
                             bool is_flattened = false);
  void DeleteObjectImpl(const DObjPath& obj_path);
  void RegisterObjectData(const std::shared_ptr<detail::ObjectData>& data);

  class Impl;
  std::unique_ptr<Impl> impl_;

  friend class detail::ObjectData;
};

}  // namespace core

}  // namespace dino
