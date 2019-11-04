// Copyright (c) 2019 Akihito Takeuchi
// Distributed under the MIT License : http://opensource.org/licenses/MIT

#pragma once

#include <QVariant>

#include "dino/core/dvalue.h"

namespace dino {

namespace qt {

QVariant DValueToQVariant(const core::DValue& value);
core::DValue QVariantToDValue(const QVariant& value);

}  // namespace Qt

}  // namespace dino
