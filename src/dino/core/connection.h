// Copyright (c) 2019 Akihito Takeuchi
// Distributed under the MIT License : http://opensource.org/licenses/MIT

#pragma once

#include <functional>
#include <boost/signals2.hpp>

#include "dino/core/dvalue.h"
#include "dino/core/command.h"

namespace dino {

namespace core {

using CommandStackListenerFunc = std::function<void ()>;
using ObjectListenerFunc = std::function<void (const Command&)>;

}  // namespace core

}  // namespace dino
