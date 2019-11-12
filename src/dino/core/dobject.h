// Copyright (c) 2018 Akihito Takeuchi
// Distributed under the MIT License : http://opensource.org/licenses/MIT

#pragma once

#include <memory>
#include <string>
#include <map>

#include "dino/core/filetypes.h"
#include "dino/core/fspath.h"
#include "dino/core/dobjpath.h"
#include "dino/core/dobjinfo.h"
#include "dino/core/dvalue.h"
#include "dino/core/connection.h"
#include "dino/core/fwd.h"

namespace dino {

namespace core {

namespace detail {

class ObjectData;

}

using DataWp = std::weak_ptr<detail::ObjectData>;

class DObject {
 public:
  DObject(const DataWp& data);
  virtual ~DObject();

  bool HasKey(const std::string& key) const;
  DValue Get(const std::string& key, const DValue& default_value) const;
  DValue Get(const std::string& key) const;
  void Put(const std::string& key, const char* str_value);
  void Put(const std::string& key, const DValue& value);
  void RemoveKey(const std::string& key);
  bool IsLocalKey(const std::string& key) const;
  bool HasNonLocalKey(const std::string& key) const;
  DObjPath WhereIsKey(const std::string& key) const;
  std::vector<std::string> Keys(bool local_only=false) const;

  bool HasAttr(const std::string& key) const;
  std::string Attr(const std::string& key) const;
  std::map<std::string, std::string> Attrs() const;
  void SetTemporaryAttr(const std::string& key, const std::string& value);
  void SetAttr(const std::string& key, const std::string& value);
  void RemoveAttr(const std::string& key);
  bool IsTemporaryAttr(const std::string& key) const;
  bool HasPersistentAttr(const std::string& key) const;
  void SetAllAttrsToBeSaved();

  std::string Name() const;
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
  virtual DObjectSp GetChildAt(size_t index,
                               OpenMode mode = OpenMode::kReadOnly) const;
  virtual DObjectSp OpenChild(const std::string& name,
                              OpenMode mode = OpenMode::kReadOnly) const;
  virtual DObjectSp CreateChild(const std::string& name,
                                const std::string& type,
                                bool is_flattened = false);
  DObjectSp Parent() const;
  uintptr_t ObjectId() const;
  void RefreshChildren();
  void SortChildren();
  bool IsFlattened() const;
  bool IsChildFlat(const std::string& name) const;
  void SetChildFlat(const std::string& name);
  void UnsetChildFlat(const std::string& name);
  void DeleteChild(const std::string& name);

  bool IsEditable() const;
  bool IsReadOnly() const;
  void SetEditable();
  void SetReadOnly();
  bool IsExpired() const;

  bool IsDirty() const;
  void SetDirty(bool dirty = true);

  void AddBase(const DObjectSp& base);
  std::vector<DObjectSp> Bases() const;
  void RemoveBase(const DObjectSp& base);

  std::vector<DObjectSp> BasesFromParent() const;

  std::vector<DObjectSp> EffectiveBases() const;

  boost::signals2::connection AddListener(
      const ObjectListenerFunc& listener, ListenerCallPoint call_point);
  void DisableSignal();
  void EnableSignal();

  CommandStackSp EnableCommandStack(bool enable = true);
  CommandStackSp GetCommandStack() const;

  void Save(bool recurse = false);

  SessionPtr GetSession();

  static bool IsObjectDir(const FsPath &path);

 protected:
  virtual void PreSaveHook();

 private:
  friend class detail::ObjectData;
  detail::ObjectData* GetData();
  
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace core

}  // namespace dino
