// Copyright (c) 2019 Akihito Takeuchi
// Distributed under the MIT License : http://opensource.org/licenses/MIT

#include "dino/core/dobjinfo.h"

#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>

#include "dino/core/dexception.h"

namespace dino {

namespace core {

DObjInfo::DObjInfo() = default;

DObjInfo::DObjInfo(const DObjPath& path, const std::string& type, bool is_actual)
    : path_(path), name_(path.LeafName()), type_(type), is_actual_(is_actual) {
}

DObjInfo::~DObjInfo() = default;

DObjPath DObjInfo::Path() const {
  return path_;
}

void DObjInfo::SetPath(const DObjPath &path) {
  path_ = path;
  name_ = path.LeafName();
}

std::string DObjInfo::Name() const {
  return name_;
}

void DObjInfo::SetName(const std::string& name) {
  name_ = name;
}

std::string DObjInfo::Type() const {
  return type_;
}

void DObjInfo::SetType(const std::string& type) {
  type_ = type;
}

bool DObjInfo::IsValid() const {
  return
      !path_.Empty()
      && path_.IsValid()
      && !name_.empty()
      && DObjPath::IsValidName(name_)
      && !type_.empty()
      && DObjPath::IsValidName(type_);
}

bool DObjInfo::IsActual() const {
  return is_actual_;
}

void DObjInfo::SetIsActual(bool is_actual) {
  is_actual_ = is_actual;
}

std::string DObjInfo::ToString(bool name_only) const {
  if (name_only)
    return name_ + ':' + type_;
  return path_.String() + ':' + type_;
}

DObjInfo DObjInfo::FromString(const std::string& info_str) {
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

bool DObjInfo::operator==(const DObjInfo& rhs) const {
  return
      path_ == rhs.path_
      && name_ == rhs.name_
      && type_ == rhs.type_
      && is_actual_ == rhs.is_actual_;
}

bool operator<(const DObjInfo& lhs, const DObjInfo& rhs) {
  return lhs.Name() < rhs.Name();
}

}  // namespace core

}  // namespace dino
