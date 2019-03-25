// Copyright (c) 2018 Akihito Takeuchi
// Distributed under the MIT License : http://opensource.org/licenses/MIT

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <boost/interprocess/sync/file_lock.hpp>

#include "dino/core/fwd.h"
#include "dino/core/fspath.h"
#include "dino/core/dobjpath.h"
#include "dino/core/dobjinfo.h"
#include "dino/core/dobjfileinfo.h"
#include "dino/core/dvalue.h"
#include "dino/core/filetypes.h"
#include "dino/core/detail/dataio.h"

namespace dino {

namespace core {

class Session;

namespace detail {

class ObjectData;
class DataIO;

using DataSp = std::shared_ptr<ObjectData>;

class ObjectData {
 public:
  ~ObjectData();

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
  bool IsFlattened() const;
  bool IsChildFlat(const std::string& name) const;
  void SetChildFlat(const std::string& name);
  void UnsetChildFlat(const std::string& name);
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

  void Save();
  void RefreshChildren();

  void IncRef();
  void DecRef(bool by_editable_ref);

  static DataSp Create(const DObjPath& obj_path,
                       const std::string& type,
                       ObjectData* parent,
                       Session* owner,
                       bool is_flattened);
  static DataSp Open(const DObjPath& obj_path,
                     const FsPath& dir_path,
                     ObjectData* parent,
                     Session* owner);
  static DObjFileInfo GetFileInfo(const FsPath& path);
  std::string DataFileName() const;
  FsPath DataFilePath() const;
  FsPath LockFilePath() const;

 private:
  ObjectData(const DObjPath& obj_path,
             const std::string& type,
             ObjectData* parent,
             Session* owner,
             bool is_flattened = false);
  ObjectData(const FsPath& dir_path,
             const DObjPath& obj_path,
             const std::string& type,
             ObjectData* parent,
             Session* owner);

  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace detail

}  // namespace core

}  // namespace dino
