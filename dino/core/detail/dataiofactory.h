// Copyright (c) 2018 Akihito Takeuchi
// Distributed under the MIT License : http://opensource.org/licenses/MIT

#pragma once

#include <memory>
#include <mutex>

#include "dino/core/detail/dataio.h"
#include "dino/core/filetypes.h"
#include "dino/core/dobjfileinfo.h"
#include "dino/core/fspath.h"

namespace dino {

namespace core {

namespace detail {

class DataIOFactory {
 public:
  ~DataIOFactory();
  DataIOFactory(const DataIOFactory&) = delete;
  DataIOFactory& operator=(const DataIOFactory&) = delete;

  DataIOPtr Create(FileFormat file_format);

  static DataIOFactory& Instance();
  static std::string DataFileName(const std::string& type,
                                  FileFormat file_format);
  static DObjFileInfo GetDataFileInfo(const FsPath& path);
  static DObjFileInfo FindDataFileInfo(const FsPath& path);

 private:
  DataIOFactory();

  static void Init();
  static std::once_flag init_flag_;
  static std::unique_ptr<DataIOFactory> instance_;
};

}  // namespace detail

}  // namespace core

}  // namespace dino
