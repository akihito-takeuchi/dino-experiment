// Copyright (c) 2019 Akihito Takeuchi
// Distributed under the MIT License : http://opensource.org/licenses/MIT

#pragma once

#include <functional>
#include <boost/signals2.hpp>

#include "dino/core/command.h"
#include "dino/core/fwd.h"

namespace dino {

namespace core {

enum class ListenerCallPoint {
  kPre,
  kPost,
  kNumCallPoint
};

using CommandStackListenerFunc = std::function<void ()>;
using ObjectListenerFunc = std::function<void (const Command&)>;
using PostCreateFunc = std::function<void (const DObjectSp&)>;

}  // namespace core

}  // namespace dino
