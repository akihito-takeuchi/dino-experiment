// Copyright (c) 2018 Akihito Takeuchi
// Distributed under the MIT License : http://opensource.org/licenses/MIT

#pragma once

#include "dino/core/dexception.h"

namespace dino {

namespace core {

class SessionException : public DException {
 public:
  SessionException(int error_id)
      : DException(error_id) {}
  SessionException(const DException& e)
      : DException(e) {}
};

}  // namespace core

}  // namespace dino
