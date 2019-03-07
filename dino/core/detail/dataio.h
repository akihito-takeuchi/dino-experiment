// Copyright (c) 2018 Akihito Takeuchi
// Distributed under the MIT License : http://opensource.org/licenses/MIT

#pragma once

#include <string>
#include <memory>
#include <map>
#include <functional>

#include "dino/core/dobjinfo.h"
#include "dino/core/fspath.h"
#include "dino/core/dvalue.h"

namespace dino {

namespace core {

namespace detail {

struct ReadDataArg;
using ReadDataArgPtr = std::shared_ptr<ReadDataArg>;

using CreateChildFunc = std::function<ReadDataArgPtr(const DObjInfo& obj_info)>;

struct ReadDataArg {
  ReadDataArg(DValueDict* v, DValueDict* a, const CreateChildFunc& f)
      : values(v), attrs(a), create_child(f) {}
  DValueDict* values;
  DValueDict* attrs;
  CreateChildFunc create_child;
};

class DataIO {
 public:
  virtual ~DataIO() = default;
  virtual void OpenForWrite(const FsPath& file_path) = 0;
  virtual void ToDataSection() = 0;
  virtual void ToAttributeSection() = 0;
  virtual void ToChildrenSection() = 0;
  virtual void ToSection(const std::string& section_name) = 0;
  virtual void ToSection(const DObjInfo& obj_info) = 0;
  virtual void ToSectionUp() = 0;
  virtual void WriteDict(const DValueDict& values) = 0;
  virtual void CloseForWrite() = 0;
  virtual void Load(const FsPath& file_path,
                    const ReadDataArgPtr& arg) = 0;
};

using DataIOPtr = std::unique_ptr<DataIO>;

}  // namespace detail

}  // namespace core

}  // namespace dino
