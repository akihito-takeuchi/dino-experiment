// Copyright (c) 2018 Akihito Takeuchi
// Distributed under the MIT License : http://opensource.org/licenses/MIT

#pragma once

#include <string>
#include <memory>
#include <mutex>

#include "dino/core/fspath.h"

namespace dino {

namespace core {

class CurrentUser {
 public:
  CurrentUser(const CurrentUser&) = delete;
  CurrentUser& operator=(const CurrentUser&) = delete;
  ~CurrentUser();
  std::string Name() const;
  bool IsWritable(const FsPath& path) const;

  static CurrentUser& Instance();

 private:
  CurrentUser();

  static void Init();
  static std::once_flag init_flag_;
  static std::unique_ptr<CurrentUser> instance_;

  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace core

}  // namespace dino
