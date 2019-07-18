// Copyright (c) 2019 Akihito Takeuchi
// Distributed under the MIT License : http://opensource.org/licenses/MIT

#pragma once

#include <vector>
#include <string>

#include "dino/core/fwd.h"
#include "dino/core/dobjinfo.h"
#include "dino/core/dvalue.h"
#include "dino/core/dobjpath.h"

namespace dino {

namespace core {

enum class CommandType {
  kAdd                  = 0b00000001,
  kUpdate               = 0b00000010,
  kDelete               = 0b00000011,
  kUnknown              = 0b00000000,
  kEditTypeMask         = 0b00000011,
  
  kValueAdd             = 0b00000101,
  kValueUpdate          = 0b00000110,
  kValueDelete          = 0b00000111,
  kValueUpdateType      = 0b00000100,

  kAddBaseObject        = 0b00001001,
  kRemoveBaseObject     = 0b00001011,
  kBaseObjectUpdateType = 0b00001000,

  kAddChild             = 0b00010001,
  kAddFlattenedChild    = 0b00010101,
  kRemoveChild          = 0b00010011,
  kChildListUpdateType  = 0b00010000
};

class Command {
 public:
  Command(CommandType type,
          const DObjPath& path,
          const std::string& key,
          const DValue& new_value,
          const DValue& prev_value,
          const DObjPath& target_object_path,
          const std::string& target_object_type,
          const std::vector<DObjInfo>& prev_children)
      : type_(static_cast<int>(type)), obj_path_(path), key_(key),
        new_value_(new_value), prev_value_(prev_value),
        target_object_path_(target_object_path),
        target_object_type_(target_object_type),
        prev_children_(prev_children) {}
  const DObjPath& ObjPath() const {
    return obj_path_;
  }
  CommandType Type() const {
    return static_cast<CommandType>(type_);
  }
  CommandType EditType() const {
    return static_cast<CommandType>(
        type_ & static_cast<int>(CommandType::kEditTypeMask));
  }
  bool IsValueUpdate() const {
    return type_ & static_cast<int>(CommandType::kValueUpdateType);
  }
  bool IsBaseObjectListUpdate() const {
    return type_ & static_cast<int>(CommandType::kBaseObjectUpdateType);
  }
  bool IsChildListUpdate() const {
    return type_ & static_cast<int>(CommandType::kChildListUpdateType);
  }
  const std::string& Description() const {
    return description_;
  }
  const std::string& Key() const {
    return key_;
  }
  const DValue& NewValue() const {
    return new_value_;
  }
  const DValue& PrevValue() const {
    return prev_value_;
  }
  DObjPath TargetObjectPath() const {
    return target_object_path_;
  }
  std::string TargetObjectName() const {
    return target_object_path_.LeafName();
  }
  std::string TargetObjectType()const {
    return target_object_type_;
  }
  const std::vector<DObjInfo>& PrevChildren() const {
    return prev_children_;
  }
 private:
  int type_;  // CommandType
  DObjPath obj_path_;
  std::string description_;
  std::string key_;
  DValue new_value_;
  DValue prev_value_;
  DObjPath target_object_path_;
  std::string target_object_type_;
  std::vector<DObjInfo> prev_children_;
};

}  // namespace core

}  // namespace dino
