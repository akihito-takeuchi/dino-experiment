// Copyright (c) 2019 Akihito Takeuchi
// Distributed under the MIT License : http://opensource.org/licenses/MIT

#pragma once

#include <dino/core/dobject.h>

class DinoTable : public dino::core::DObject {
 public:
  using dino::core::DObject::DObject;
  static void Init();
};

class DinoTableRow : public dino::core::DObject {
 public:
  using dino::core::DObject::DObject;
};

extern const char* kDinoTableType;
extern const char* kDinoTableRowType;
