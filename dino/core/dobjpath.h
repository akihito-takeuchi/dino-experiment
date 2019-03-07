// Copyright (c) 2018 Akihito Takeuchi
// Distributed under the MIT License : http://opensource.org/licenses/MIT

#pragma once

#include <string>
#include <vector>
#include <regex>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/filesystem.hpp>

namespace dino {

namespace core {

namespace {

std::regex kObjectNamePattern(R"(\w[0-9a-zA-Z_]*)");

}

class DObjPath {
 public:
  DObjPath() = default;
  explicit DObjPath(const std::string& path_str) {
    boost::algorithm::split(path_elems_, path_str, boost::is_any_of("/"));
    path_elems_.erase(
        std::remove(path_elems_.begin(), path_elems_.end(), ""),
        path_elems_.end());
  }
  DObjPath(const DObjPath&) = default;
  DObjPath(DObjPath&&) = default;
  DObjPath& operator=(const DObjPath& obj_path) = default;

  bool IsValid() const {
    for (auto elem : path_elems_)
      if (!IsValidName(elem))
        return false;
    return true;
  }
  DObjPath ChildPath(const std::string& child_name) const {
    DObjPath child_path(*this);
    child_path.path_elems_.emplace_back(child_name);
    return child_path;
  }
  std::string String() const {
    return boost::algorithm::join(path_elems_, "/");
  }
  boost::filesystem::path DirPath() const {
    boost::filesystem::path path;
    for (auto elem : path_elems_)
      path /= elem;
    return path;
  }
  bool IsTop() const {
    return path_elems_.size() == 1;
  }
  size_t Depth() const {
    return path_elems_.size();
  }
  bool Empty() const {
    if (path_elems_.empty())
      return true;
    return std::find_if(
        path_elems_.cbegin(), path_elems_.cend(),
        [](auto& e) { return e.empty(); }) != path_elems_.end();
  }
  std::string TopName() const {
    return path_elems_.front();
  }
  DObjPath Top() const {
    return DObjPath(TopName());
  }
  DObjPath Tail() const {
    DObjPath tail(*this);
    tail.path_elems_.erase(tail.path_elems_.begin());
    return tail;
  }
  DObjPath ParentPath() const {
    DObjPath parent(*this);
    parent.path_elems_.pop_back();
    return parent;
  }
  std::string LeafName() const {
    return path_elems_.back();
  }
  DObjPath Leaf() const {
    return DObjPath(LeafName());
  }
  bool operator==(const DObjPath& other) const {
    return String() == other.String();
  }
  bool operator!=(const DObjPath& other) const {
    return !(*this == other);
  }

  static bool IsValidName(const std::string& name) {
    return std::regex_match(name, kObjectNamePattern);
  }

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
