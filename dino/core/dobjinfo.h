// Copyright (c) 2018 Akihito Takeuchi
// Distributed under the MIT License : http://opensource.org/licenses/MIT

#pragma once

#include <string>
#include <boost/algorithm/string/split.hpp>

#include "dino/core/dobjpath.h"
#include "dino/core/filetypes.h"
#include "dino/core/dexception.h"

namespace dino {

namespace core {

class DObjInfo {
 public:
  DObjInfo() = default;
  DObjInfo(const DObjPath& path,
           const std::string& type,
           bool is_local = true)
      : path_(path), name_(path.LeafName()), type_(type),
        is_local_(is_local) {}
  ~DObjInfo() = default;
  DObjPath Path() const {
    return path_;
  }
  void SetPath(const DObjPath &path) {
    path_ = path;
    name_ = path.LeafName();
  }
  std::string Name() const {
    return name_;
  }
  void SetName(const std::string& name) {
    name_ = name;
  }
  std::string Type() const {
    return type_;
  }
  void SetType(const std::string& type) {
    type_ = type;
  }
  bool IsValid() const {
    return
        !path_.Empty()
        && path_.IsValid()
        && !name_.empty()
        && DObjPath::IsValidName(name_)
        && !type_.empty()
        && DObjPath::IsValidName(type_);
  }
  bool IsLocal() const {
    return is_local_;
  }
  void SetIsLocal(bool is_local) {
    is_local_ = is_local;
  }
  std::string ToString(bool name_only = false) const {
    if (name_only)
      return name_ + ':' + type_;
    return path_.String() + ':' + type_;
  }
  static DObjInfo FromString(const std::string& info_str) {
    std::vector<std::string> elems;
    boost::split(elems, info_str, boost::is_any_of(":"));
    if (elems.size() != 2)
      BOOST_THROW_EXCEPTION(
          DException(kErrInvalidObjectString)
          << ExpInfo1(info_str));
    DObjPath path(elems[0]);
    if (!path.IsValid() || !DObjPath::IsValidName(elems[1]))
      BOOST_THROW_EXCEPTION(
          DException(kErrInvalidObjectString)
          << ExpInfo1(info_str));
    return DObjInfo(path, elems[1]);
  }
  bool operator==(const DObjInfo& rhs) const {
    return
        path_ == rhs.path_
        && name_ == rhs.name_
        && type_ == rhs.type_
        && is_local_ == rhs.is_local_;
  }

 private:
  DObjPath path_;
  std::string name_;
  std::string type_;
  bool is_local_ = true;
};

inline bool operator<(const DObjInfo& lhs, const DObjInfo& rhs) {
  return lhs.Name() < rhs.Name();
}

}  // namespace core

}  // namespace dino
