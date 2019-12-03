// Copyright (c) 2018 Akihito Takeuchi
// Distributed under the MIT License : http://opensource.org/licenses/MIT

#pragma once

#include <string>
#include <vector>
#include <functional>

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
  using PreOpenHookFuncType = std::function<void (const DObjPath&, OpenMode)>;
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
  DObjectSp GetObjectById(uintptr_t object_id,
                          OpenMode mode = OpenMode::kReadOnly) const;
  bool IsOpened(const DObjPath& obj_path) const;
  void DeleteObject(const DObjPath& obj_path);
  void RemoveTopLevelObject(const std::string& name, bool delete_files = false);
  void PurgeObject(const DObjPath& obj_path);
  void SetPreOpenHook(const PreOpenHookFuncType& pre_open_hook);

  static SessionPtr Create();

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
