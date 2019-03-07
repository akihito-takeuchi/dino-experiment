// Copyright (c) 2018 Akihito Takeuchi
// Distributed under the MIT License : http://opensource.org/licenses/MIT

#pragma once

#include <string>
#include <unordered_map>
#include <boost/variant.hpp>

namespace dino {

namespace core {

class DNil {
 public:
  bool operator==(const DNil&) const {
    return true;
  }
  bool operator!=(const DNil&) const {
    return false;
  }
};

namespace detail {

class DValueEquals : public boost::static_visitor<bool> {
 public:
  template<typename T, typename U>
  bool operator()(const T&, const U&) const {
    return false;
  }
  template<typename T>
  bool operator()(const T& lhs, const T& rhs) const {
    return lhs == rhs;
  }
};

using DValueBase = boost::variant<DNil, bool, std::string, double, int>;

}  // namespace detail

class DValue : public detail::DValueBase {
 public:
  DValue() : detail::DValueBase() {}
  template<typename T>
  DValue(const T& v) : detail::DValueBase(v) {}
  DValue(const char* v) : detail::DValueBase(std::string(v)) {}
  
  bool operator==(const DValue& other) const {
    return boost::apply_visitor(detail::DValueEquals(), *this, other);
  }
  bool operator!=(const DValue& other) const {
    return !boost::apply_visitor(detail::DValueEquals(), *this, other);
  }
};

using DValueDict = std::unordered_map<std::string, DValue>;

}  // namespace core

}  // namespace dino
