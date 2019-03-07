// Copyright (c) 2018 Akihito Takeuchi
// Distributed under the MIT License : http://opensource.org/licenses/MIT

#pragma once

#include <string>
#include <vector>

#include "dino/core/fspath.h"
#include "dino/core/dobjpath.h"
#include "dino/core/ptrdefs.h"
#include "dino/core/filetypes.h"

namespace dino {

namespace core {

namespace detail {

class ObjectData;

}  // namespace detail

class Session {
 public:
  Session(const Session&) = delete;
  Session& operator=(const Session&) = delete;
  ~Session();
  std::vector<std::string> TopObjectNames() const;
  DObjectSp CreateTopLevelObject(const std::string& name,
                                 const std::string& type);
  DObjectSp OpenTopLevelObject(const FsPath& dir_path,
                              const std::string& name);
  void InitTopLevelObjectPath(const std::string& name,
                              const FsPath& dir_path);
  DObjectSp CreateObject(const DObjPath& obj_info,
                         const std::string& type,
                         bool is_flattened = false);
  DObjectSp OpenObject(const DObjPath& obj_path);
  DObjectSp GetObject(const DObjPath& obj_path) const;
  FsPath WorkspaceFilePath() const;
  void PurgeObject(const DObjPath& obj_path);
  void Save();
  bool HasError();
  std::string ErrorMessage() const;
  void ClearErrorMessage();

  void RegisterObjectData(const std::shared_ptr<detail::ObjectData>& data);

  static SessionPtr Create();
  static SessionPtr Create(const std::string& wsp_file_path);
  static SessionPtr Create(const FsPath& wsp_file_path);
  static SessionPtr Open(const FsPath& wsp_file_path);

 private:
  Session();

  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace core

}  // namespace dino
