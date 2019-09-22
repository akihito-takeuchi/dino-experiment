// Copyright (c) 2018 Akihito Takeuchi
// Distributed under the MIT License : http://opensource.org/licenses/MIT

#pragma once

#include <string>

#include "dino/core/dobjpath.h"

namespace dino {

namespace core {

class DObjInfo {
 public:
  DObjInfo();
  DObjInfo(const DObjPath& path, const std::string& type, bool is_local = true);
  ~DObjInfo();
  DObjInfo(const DObjInfo&) = default;
  DObjInfo& operator=(const DObjInfo&) = default;

  DObjPath Path() const;
  void SetPath(const DObjPath &path);
  std::string Name() const;
  void SetName(const std::string& name);
  std::string Type() const;
  void SetType(const std::string& type);
  bool IsValid() const;
  bool IsLocal() const;
  void SetIsLocal(bool is_local);
  std::string ToString(bool name_only = false) const;
  static DObjInfo FromString(const std::string& info_str);
  bool operator==(const DObjInfo& rhs) const;

 private:
  DObjPath path_;
  std::string name_;
  std::string type_;
  bool is_local_ = false;
};

bool operator<(const DObjInfo& lhs, const DObjInfo& rhs);

}  // namespace core

}  // namespace dino
