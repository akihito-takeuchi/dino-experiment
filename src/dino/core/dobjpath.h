// Copyright (c) 2018 Akihito Takeuchi
// Distributed under the MIT License : http://opensource.org/licenses/MIT

#pragma once

#include <string>
#include <vector>

#include "dino/core/fspath.h"

namespace dino {

namespace core {

class DObjPath {
 public:
  DObjPath();
  DObjPath(const std::string& path_str);
  DObjPath(const DObjPath&);
  DObjPath(DObjPath&&);
  DObjPath& operator=(const DObjPath& obj_path);

  bool IsValid() const;
  DObjPath ChildPath(const std::string& child_name) const;
  std::string String() const;
  FsPath DirPath() const;
  bool IsTop() const;
  size_t Depth() const;
  bool Empty() const;
  std::string TopName() const;
  DObjPath Top() const;
  DObjPath Tail() const;
  DObjPath ParentPath() const;
  std::string LeafName() const;
  DObjPath Leaf() const;
  bool operator==(const DObjPath& other) const;
  bool operator!=(const DObjPath& other) const;

  static bool IsValidName(const std::string& name);

  struct Hash {
    using result_type = std::size_t;
    std::size_t operator()(const DObjPath& path) const {
      return std::hash<std::string>()(path.String());
    }
  };

 private:
  std::vector<std::string> path_elems_;
};

}  // namespace core

}  // namespace dino
