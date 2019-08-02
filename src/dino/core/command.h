// Copyright (c) 2019 Akihito Takeuchi
// Distributed under the MIT License : http://opensource.org/licenses/MIT

#pragma once

#include <vector>
#include <string>
#include <fmt/format.h>

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
  kDeleteChild          = 0b00010011,
  kChildListUpdateType  = 0b00010000
};

inline std::string CommandTypeToString(CommandType type) {
  std::string result;
  switch (type) {
    case CommandType::kAdd:               result = "Add";               break;
    case CommandType::kUpdate:            result = "Update";            break;
    case CommandType::kDelete:            result = "Delete";            break;
    case CommandType::kUnknown:           result = "Unknown";           break;
    case CommandType::kValueAdd:          result = "ValueAdd";          break;
    case CommandType::kValueUpdate:       result = "ValueUpdate";       break;
    case CommandType::kValueDelete:       result = "ValueDelete";       break;
    case CommandType::kAddBaseObject:     result = "AddBaseObject";     break;
    case CommandType::kRemoveBaseObject:  result = "RemoveBaseObject";  break;
    case CommandType::kAddChild:          result = "AddChild";          break;
    case CommandType::kAddFlattenedChild: result = "AddFlattenedChild"; break;
    case CommandType::kDeleteChild:       result = "DeleteChild";       break;
    default:
      result = fmt::format("UnknownCommandType({0})", static_cast<int>(type));
  }
  return result;
}

class Command {
 public:
  Command() = default;
  Command(const Command&) = default;
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
