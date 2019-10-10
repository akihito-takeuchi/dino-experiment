// Copyright (c) 2019 Akihito Takeuchi
// Distributed under the MIT License : http://opensource.org/licenses/MIT

#pragma once

#include <string>
#include <memory>
#include <mutex>

#include "dino/core/dobject.h"
#include "dino/core/dexception.h"

namespace dino {

namespace core {

class ObjectFactory {
 public:
  enum class ObjectFlatTypeConst {
    kFlattened,
    kNotFlattened,
    kSpecifyAtCreation
  };
  using CreateFunc = std::function<DObject* (const DataWp& data)>;
  using GetObjFunc = std::function<DObjectSp (const DObjInfo& info)>;
  using ChildrenSortCompareFunc = std::function<
    bool (const GetObjFunc& func, const DObjInfo& lhs, const DObjInfo& rhs)>;

  ObjectFactory(const ObjectFactory&) = delete;
  ObjectFactory& operator=(const ObjectFactory&) = delete;
  ~ObjectFactory();
  bool Register(
      const std::string& type,
      const CreateFunc& func,
      ObjectFlatTypeConst flat_type = ObjectFlatTypeConst::kSpecifyAtCreation);
  bool RegisterChildrenSortCompareFunc(const std::string& type,
                                       const ChildrenSortCompareFunc& comp);
  bool SetDefaultCreateFunc(const CreateFunc& func);
  DObject* Create(const DataWp& data) const;
  ChildrenSortCompareFunc GetChildrenSortCompareFunc(
      const std::string& type) const;
  ObjectFlatTypeConst FlatType(const std::string& type) const;
  bool IsFlattenedObject(const std::string& type) const;
  bool UpdateFlattenedFlag(const std::string& type, bool is_flattened) const;
  void EnableDefault();
  void DisableDefault();
  void Reset();

  static ObjectFactory& Instance();

 private:
  ObjectFactory();

  static void Init();
  static std::once_flag init_flag_;
  static std::unique_ptr<ObjectFactory> instance_;

  class Impl;
  std::unique_ptr<Impl> impl_;
};

class ObjectFactoryException : public DException {
 public:
  using DException::DException;
};

}  // namespace core

}  // namespace dino
