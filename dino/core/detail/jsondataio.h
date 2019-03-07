// Copyright (c) 2018 Akihito Takeuchi
// Distributed under the MIT License : http://opensource.org/licenses/MIT

#pragma once

#include <memory>

#include "dino/core/detail/dataio.h"
#include "dino/core/dvalue.h"
#include "dino/core/dobjinfo.h"
#include "dino/core/dobjfileinfo.h"

namespace dino {

namespace core {

namespace detail {

class JsonDataIO : public DataIO {
 public:
  JsonDataIO();
  ~JsonDataIO();
  virtual void OpenForWrite(const FsPath& file_path) override;
  virtual void ToDataSection() override;
  virtual void ToAttributeSection() override;
  virtual void ToChildrenSection() override;
  virtual void ToSection(const std::string& section_name) override;
  virtual void ToSection(const DObjInfo& obj_info) override;
  virtual void ToSectionUp() override;
  virtual void WriteDict(const DValueDict& values) override;
  virtual void CloseForWrite() override;
  virtual void Load(const FsPath& file_path,
                    const ReadDataArgPtr& arg) override;
  static std::string FileName(const std::string& type);
  static DObjFileInfo GetDataFileInfo(const FsPath& path_str);

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace detail

}  // namespace core

}  // namespace dino
