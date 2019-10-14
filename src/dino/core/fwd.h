// Copyright (c) 2018 Akihito Takeuchi
// Distributed under the MIT License : http://opensource.org/licenses/MIT

#pragma once

#include <memory>

namespace dino {

namespace core {

class Session;
using SessionPtr = std::shared_ptr<Session>;

class DObject;
using DObjectSp = std::shared_ptr<DObject>;

class CommandExecuter;
using CommandExecuterSp = std::shared_ptr<CommandExecuter>;

class CommandStack;
using CommandStackSp = std::shared_ptr<CommandStack>;

}  // namespace core

}  // namespace dino
