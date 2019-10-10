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
#include "dino/core/connection.h"
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
  bool IsLocalKey(const std::string& key) const;
  DObjPath WhereIsKey(const std::string& key) const;
  std::vector<std::string> Keys(bool local_only=false) const;

  bool HasAttr(const std::string& key) const;
  std::string Attr(const std::string& key) const;
  std::map<std::string, std::string> Attrs() const;
  void SetTemporaryAttr(const std::string& key, const std::string& value);
  void SetAttr(const std::string& key, const std::string& value);
  void SetAllAttrsToBeSaved();
  void RemoveAttr(const std::string& key);
  bool IsTemporaryAttr(const std::string& key) const;
  bool HasPersistentAttr(const std::string& key) const;

  std::string Type() const;
  FsPath DirPath() const;
  DObjPath Path() const;

  bool IsActual() const;
  bool HasChild(const std::string& name) const;
  bool HasActualChild(const std::string& name) const;
  bool IsActualChild(const std::string& name) const;
  bool IsChildOpened(const std::string& name) const;
  std::vector<DObjInfo> Children() const;
  DObjInfo ChildInfo(const std::string& name) const;
  size_t ChildCount() const;
  bool IsFlattened() const;
  bool IsChildFlat(const std::string& name) const;
  void SetChildFlat(const std::string& name);
  void UnsetChildFlat(const std::string& name);
  DObjectSp GetChild(const std::string& name, OpenMode mode) const;
  DObjectSp OpenChild(const std::string& name, OpenMode mode) const;
  DObjectSp CreateChild(const std::string& name,
                        const std::string& type,
                        bool is_flattened);
  DObjectSp Parent() const;
  uintptr_t ObjectId() const;
  void RemoveChild(const std::string& name);
  void AddChildInfo(const DObjInfo& child_info);
  void DeleteChild(const std::string& name);

  void AcquireWriteLock();
  void ReleaseWriteLock();

  void InitDirPath(const FsPath& dir_path);

  bool IsDirty() const;
  void SetDirty(bool dirty);
  bool IsEditable() const;

  void AddBase(const DObjectSp& base);
  std::vector<DObjectSp> Bases() const;
  void RemoveBase(const DObjectSp& base);

  void AddBaseFromParent(const DObjectSp& base);
  std::vector<DObjectSp> BasesFromParent() const;

  std::vector<DObjectSp> EffectiveBases() const;

  boost::signals2::connection AddListener(
      const ObjectListenerFunc& listener, ListenerCallPoint call_point);
  void DisableSignal();
  void EnableSignal();

  CommandStackSp EnableCommandStack(bool enable);
  CommandStackSp GetCommandStack() const;

  void Save(bool recurse);
  void RefreshChildren();

  void IncRef();
  void DecRef(bool by_editable_ref);

  ObjectData* GetDataAt(const DObjPath& path);

  ConstSessionPtr GetSession() const;

  static DataSp Create(const DObjPath& obj_path,
                       const std::string& type,
                       ObjectData* parent,
                       Session* owner,
                       bool is_flattened,
                       bool init_directory = true,
                       bool is_actual = true);
  static DataSp Open(const DObjPath& obj_path,
                     const FsPath& dir_path,
                     ObjectData* parent,
                     Session* owner);
  static DObjFileInfo GetFileInfo(const FsPath& path);
  std::string DataFileName() const;
  FsPath DataFilePath() const;
  FsPath LockFilePath() const;

  // Do actually
  void ExecUpdateValue(const std::string& key,
                       const DValue& new_value,
                       const DValue& prev_value);
  void ExecRemoveValue(const std::string& key,
                       const DValue& prev_value);
  void ExecAddValue(const std::string& key,
                    const DValue& new_value);
  DObjectSp ExecCreateChild(const std::string& name,
                            const std::string& type,
                            bool is_flattened);
  void ExecDeleteChild(const std::string& name);
  void ExecAddBase(const DObjectSp& base);
  void ExecRemoveBase(const DObjectSp& base);

  void Load();

 private:
  ObjectData(const DObjPath& obj_path,
             const std::string& type,
             ObjectData* parent,
             Session* owner,
             bool is_flattened = false,
             bool init_directory = true,
             bool is_actual = true);
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
