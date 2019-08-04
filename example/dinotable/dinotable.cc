// Copyright (c) 2019 Akihito Takeuchi
// Distributed under the MIT License : http://opensource.org/licenses/MIT

#include "dinotable.h"

#include <dino/core/objectfactory.h>

const char* kDinoTableType = "dinotable";
const char* kDinoTableRowType = "tablerow";

void DinoTable::Init() {
  dino::core::ObjectFactory::Instance().Register(
      kDinoTableType,
      [](auto& data) { return new DinoTable(data); },
      dino::core::ObjectFactory::ObjectFlatTypeConst::kFlattened);
  dino::core::ObjectFactory::Instance().Register(
      kDinoTableRowType,
      [](auto& data) { return new DinoTableRow(data); },
      dino::core::ObjectFactory::ObjectFlatTypeConst::kFlattened);
}
