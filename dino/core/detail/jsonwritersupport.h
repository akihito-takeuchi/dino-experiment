// Copyright (c) 2018 Akihito Takeuchi
// Distributed under the MIT License : http://opensource.org/licenses/MIT

#pragma once

#include "dino/core/detail/jsonexception.h"
#include "dino/core/dvalue.h"

namespace dino {

namespace core {

namespace detail {

template<typename W>
class WriteData : public boost::static_visitor<bool> {
 public:
  WriteData(W& writer) : writer_(writer) {}
  template<typename T>
  bool operator()(const T&) {
    BOOST_THROW_EXCEPTION(JsonException(kErrJsonWriteData));
    return false;
  }
  bool operator()(const DNilType&) {
    return writer_.Null();
  }
  bool operator()(bool value) {
    return writer_.Bool(value);
  }
  bool operator()(int value) {
    return writer_.Int(value);
  }
  bool operator()(long int value) {
    return writer_.Int64(value);
  }
  bool operator()(double value) {
    return writer_.Double(value);
  }
  bool operator()(const std::string& value) {
    return writer_.String(value.c_str());
  }

 private:
  W& writer_;
};

}  // namespace detail

}  // namespace core

}  // namespace detail
