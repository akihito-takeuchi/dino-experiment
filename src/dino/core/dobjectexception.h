// Copyright (c) 2018 Akihito Takeuchi
// Distributed under the MIT License : http://opensource.org/licenses/MIT

#pragma once

#include "dino/core/dexception.h"

namespace dino {

namespace core {

class DObjectException : public DException {
 public:
  DObjectException(int error_id)
      : DException(error_id) {}
  DObjectException(const DException& e)
      : DException(e) {}
};

}  // namespace core

}  // namespace dino
