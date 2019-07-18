// Copyright (c) 2018 Akihito Takeuchi
// Distributed under the MIT License : http://opensource.org/licenses/MIT

#pragma once

#include <string>
#include <boost/filesystem.hpp>

#include "dino/core/dobjpath.h"
#include "dino/core/filetypes.h"
#include "dino/core/fspath.h"

namespace dino {

namespace core {

class DObjFileInfo {
 public:
  DObjFileInfo() = default;
  DObjFileInfo(const std::string& type,
               const FsPath& file_path,
               FileFormat format)
      : type_(type), file_path_(file_path), format_(format) {
  }
  ~DObjFileInfo() = default;
  std::string DirName() const {
    return file_path_.parent_path().filename().string();
  }
  std::string Type() const {
    return type_;
  }
  bool IsValid() const {
    return
        DObjPath::IsValidName(type_)
        && format_ != FileFormat::kUnknown
        && format_ != FileFormat::kNone
        && boost::filesystem::exists(file_path_);
  }

 private:
  std::string type_;
  FsPath file_path_;
  FileFormat format_ = FileFormat::kNone;
};

}  // namespace core

}  // namespace dino
