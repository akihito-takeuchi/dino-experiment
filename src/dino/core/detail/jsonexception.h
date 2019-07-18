// Copyright (c) 2018 Akihito Takeuchi
// Distributed under the MIT License : http://opensource.org/licenses/MIT

#pragma once

#include "dino/core/dexception.h"

namespace dino {

namespace core {

namespace detail {

class JsonException : public DException {
 public:
  JsonException(int error_id)
      : DException(error_id) {}
  JsonException(const DException& e)
      : DException(e) {}
};

extern const int kErrJsonWriteData;
extern const int kErrJsonFileOpen;
extern const int kErrJsonInvalidSectionName;
extern const int kErrJsonFailedToChangeSection;
extern const int kErrJsonInvalidFileReadState;

}  // namespace detail

}  // namespace core

}  // namespace dino
