// Copyright (c) 2018 Akihito Takeuchi
// Distributed under the MIT License : http://opensource.org/licenses/MIT

#pragma once

#include <memory>

#include "dino/core/filetypes.h"
#include "dino/core/fspath.h"
#include "dino/core/dobjpath.h"
#include "dino/core/dobjinfo.h"
#include "dino/core/dvalue.h"
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
  void Put(const std::string& key, const DValue& value);
  void RemoveKey(const std::string& key);
  bool IsLocal(const std::string& key) const;
  DObjPath Where(const std::string& key) const;

  std::string Name() const;
  std::string Type() const;
  FsPath DirPath() const;
  DObjPath Path() const;

  bool HasChild(const std::string& name) const;
  bool HasLocalChild(const std::string& name) const;
  bool IsLocalChild(const std::string& name) const;
  std::vector<DObjInfo> Children() const;
  size_t ChildCount() const;
  DObjectSp GetChildObject(size_t index) const;
  DObjectSp GetChildObject(const std::string& name) const;
  DObjectSp CreateChild(const std::string& name,
                        const std::string& type,
                        bool is_flattened = false);
  void RefreshChildren();
  bool IsFlattened() const;
  bool IsChildFlat(const std::string& name) const;
  void SetChildFlat(const std::string& name);
  void UnsetChildFlat(const std::string& name);

  bool IsEditable() const;
  bool IsReadOnly() const;
  void SetEditable();
  void SetReadOnly();
  bool IsExpired() const;

  bool IsDirty() const;
  void SetDirty(bool dirty = true);

  void AddBase(const DObjectSp& base);
  std::vector<DObjectSp> BaseObjects() const;
  void RemoveBase(const DObjectSp& base);

  void Save();

  static bool IsObjectDir(const FsPath &path);

 private:
  friend class detail::ObjectData;
  detail::ObjectData* GetData();
  
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace core

}  // namespace dino
