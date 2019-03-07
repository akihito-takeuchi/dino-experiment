// Copyright (c) 2018 Akihito Takeuchi
// Distributed under the MIT License : http://opensource.org/licenses/MIT

#pragma once

#include <boost/filesystem.hpp>

namespace dino {

namespace core {

using FsPath = boost::filesystem::path;

inline FsPath ParentFsPath(const FsPath& path) {
  auto parent_path = path.parent_path();
  if (parent_path.empty())
    parent_path = ".";
  return parent_path;
}

}  // namespace core

}  // namespace dino
