// Copyright (c) 2019 Akihito Takeuchi
// Distributed under the MIT License : http://opensource.org/licenses/MIT

#include "dino/core/dobjpath.h"

#include <regex>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/classification.hpp>

namespace dino {

namespace core {

namespace {

std::regex kObjectNamePattern(R"(\w+)");

}  // namespace

DObjPath::DObjPath() = default;

DObjPath::DObjPath(const DObjPath&) = default;

DObjPath::DObjPath(DObjPath&&) = default;

DObjPath::DObjPath(const std::string& path_str) {
  boost::algorithm::split(path_elems_, path_str, boost::is_any_of("/"));
  path_elems_.erase(
      std::remove(path_elems_.begin(), path_elems_.end(), ""),
      path_elems_.end());
}

DObjPath& DObjPath::operator=(const DObjPath& obj_path) = default;

bool DObjPath::IsValid() const {
  for (auto elem : path_elems_)
    if (!IsValidName(elem))
      return false;
  return true;
}

DObjPath DObjPath::ChildPath(const std::string& child_name) const {
  DObjPath child_path(*this);
  child_path.path_elems_.emplace_back(child_name);
  return child_path;
}

std::string DObjPath::String() const {
  return boost::algorithm::join(path_elems_, "/");
}

boost::filesystem::path DObjPath::DirPath() const {
  boost::filesystem::path path;
  for (auto elem : path_elems_)
    path /= elem;
  return path;
}

bool DObjPath::IsTop() const {
  return path_elems_.size() == 1;
}

size_t DObjPath::Depth() const {
  return path_elems_.size();
}

bool DObjPath::Empty() const {
  if (path_elems_.empty())
    return true;
  return std::find_if(
      path_elems_.cbegin(), path_elems_.cend(),
      [](auto& e) { return e.empty(); }) != path_elems_.end();
}

std::string DObjPath::TopName() const {
  return path_elems_.front();
}

DObjPath DObjPath::Top() const {
  return DObjPath(TopName());
}

DObjPath DObjPath::Tail() const {
  DObjPath tail(*this);
  tail.path_elems_.erase(tail.path_elems_.begin());
  return tail;
}

DObjPath DObjPath::ParentPath() const {
  DObjPath parent(*this);
  parent.path_elems_.pop_back();
  return parent;
}

std::string DObjPath::LeafName() const {
  return path_elems_.back();
}

bool DObjPath::IsDescendantOf(
    const DObjPath& ancestor, bool include_self) const {
  if (include_self && ancestor == *this)
    return true;
  if (ancestor.path_elems_.size() >= path_elems_.size())
    return false;
  for (size_t idx = 0; idx < ancestor.path_elems_.size(); ++ idx)
    if (ancestor.path_elems_[idx] != path_elems_[idx])
      return false;
  return true;
}

void DObjPath::Clear() {
  path_elems_.clear();
}

DObjPath DObjPath::Leaf() const {
  return DObjPath(LeafName());
}

bool DObjPath::operator==(const DObjPath& other) const {
  return String() == other.String();
}

bool DObjPath::operator!=(const DObjPath& other) const {
  return !(*this == other);
}

bool DObjPath::IsValidName(const std::string& name) {
  return std::regex_match(name, kObjectNamePattern);
}

}  // namespace core

}  // namespace dino
