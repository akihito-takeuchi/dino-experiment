// Copyright (c) 2018 Akihito Takeuchi
// Distributed under the MIT License : http://opensource.org/licenses/MIT

#pragma once

#include "dino/core/dexception.h"

namespace dino {

namespace core {

namespace detail {

class DataIOException : public DException {
 public:
  DataIOException(int error_id)
      : DException(error_id) {}
  DataIOException(const DException& e)
      : DException(e) {}
};

extern const int kErrUnknownFileFormat;

}  // namespace detail

}  // namespace core

}  // namespace dino
