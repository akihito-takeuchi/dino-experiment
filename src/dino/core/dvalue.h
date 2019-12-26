// Copyright (c) 2018 Akihito Takeuchi
// Distributed under the MIT License : http://opensource.org/licenses/MIT

#pragma once

#include <string>
#include <unordered_map>
#include <fmt/format.h>
#include <boost/variant.hpp>
#include <boost/algorithm/string/join.hpp>

namespace dino {

namespace core {

class DNilType {
 public:
  bool operator==(const DNilType&) const {
    return true;
  }
  bool operator!=(const DNilType&) const {
    return false;
  }
};

const DNilType nil;

using DValue = boost::make_recursive_variant<
  DNilType, bool, std::string, double, int,
  std::vector<boost::recursive_variant_>>::type;

using DValueArray = std::vector<DValue>;

using DValueDict = std::unordered_map<std::string, DValue>;

inline bool IsArrayValue(const DValue& v) {
  return v.type() == typeid(DValueArray);
}

#define DEFINE_DVALUE_EQ_OPERATOR(tname)                                \
  inline bool operator==(const DValue& lhs, const tname& rhs) {         \
    return lhs.type() == typeid(rhs) && boost::get<tname>(lhs) == rhs;  \
  }                                                                     \
  inline bool operator==(const tname& lhs, const DValue& rhs) {         \
    return rhs == lhs;                                                  \
  }

#define DEFINE_DVALUE_NE_OPERATOR(tname)                                \
  inline bool operator!=(const DValue& lhs, const tname& rhs) {         \
    return !(lhs == rhs);                                               \
  }                                                                     \
  inline bool operator!=(const tname& lhs, const DValue& rhs) {         \
    return !(rhs == lhs);                                               \
  }

#define DEFINE_DVALUE_EQ_NE_OPERATOR(tname)     \
  DEFINE_DVALUE_EQ_OPERATOR(tname);             \
  DEFINE_DVALUE_NE_OPERATOR(tname);

DEFINE_DVALUE_EQ_NE_OPERATOR(DNilType);
DEFINE_DVALUE_EQ_NE_OPERATOR(bool);
DEFINE_DVALUE_EQ_NE_OPERATOR(std::string);
DEFINE_DVALUE_EQ_NE_OPERATOR(double);
DEFINE_DVALUE_EQ_NE_OPERATOR(int);

inline bool operator==(const DValue& lhs, const DValueArray& rhs) {
  if (lhs.type() != typeid(rhs))
    return false;
  auto lhs_val = boost::get<DValueArray>(lhs);
  if (lhs_val.size() != rhs.size())
    return false;
  auto l_itr = lhs_val.cbegin();
  auto r_itr = rhs.cbegin();
  for (; l_itr != lhs_val.cend(); ++ l_itr, ++ r_itr)
    if (*l_itr != *r_itr)
      return false;
  return true;
}

inline bool operator==(const DValueArray& lhs, const DValue& rhs) {
  return rhs == lhs;
}

DEFINE_DVALUE_NE_OPERATOR(DValueArray);

inline bool operator==(const DValue& lhs, const char* rhs) {
  return lhs == std::string(rhs);
}

inline bool operator==(const char* lhs, const DValue& rhs) {
  return rhs == std::string(lhs);
}

inline bool operator!=(const DValue& lhs, const char* rhs) {
  return !(lhs == rhs);
}

inline bool operator!=(const char* lhs, const DValue& rhs) {
  return !(rhs == lhs);
}

inline DValueArray ToDValueArray(const DValue& value) {
  DValueArray values;
  if (value == nil)
    return values;
  try {
    values = boost::get<DValueArray>(value);
  } catch (const boost::bad_get&) {
    values.push_back(value);
  }
  return values;
}

namespace detail {

class DValueToString : public boost::static_visitor<std::string> {
 public:
  DValueToString(char sep, char left_paren, char right_paren)
      : left_paren_(left_paren), right_paren_(right_paren) {
    sep_[0] = sep;
    sep_[1] = 0;
  }
  std::string operator()(DNilType) const {
    return "nil";
  }
  std::string operator()(bool v) const {
    if (v)
      return "true";
    else
      return "false";
  }
  std::string operator()(const std::string& v) const {
    return '"' + v + '"';
  }
  std::string operator()(double v) const {
    return fmt::format("{:.1f}", v);
  }
  std::string operator()(int v) const {
    return fmt::format("{}", v);
  }
  std::string operator()(const std::vector<DValue>& values) const {
    std::vector<std::string> result_strs;
    std::transform(
        values.cbegin(), values.cend(),
        std::back_inserter(result_strs),
        [this](auto& v) { return boost::apply_visitor(
            detail::DValueToString(sep_[0], left_paren_, right_paren_), v); });
    return
        left_paren_
        + boost::algorithm::join(result_strs, sep_)
        + right_paren_;
  }
 private:
  char sep_[2];
  char left_paren_;
  char right_paren_;
};

}  // namespace detail

inline std::string ToString(const DValue& value,
                            char sep = ',',
                            char left_paren = '[',
                            char right_paren = ']') {
  return boost::apply_visitor(
      detail::DValueToString(sep, left_paren, right_paren), value);
}

inline std::ostream& operator<<(std::ostream& os, const DValue& value) {
  os << ToString(value);
  return os;
}

}  // namespace core

}  // namespace dino
