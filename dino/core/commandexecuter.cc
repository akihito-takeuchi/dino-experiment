// Copyright (c) 2019 Akihito Takeuchi
// Distributed under the MIT License : http://opensource.org/licenses/MIT

#include "dino/core/commandexecuter.h"
#include "dino/core/detail/objectdata.h"

namespace dino {

namespace core {

CommandExecuter::CommandExecuter(Session* session, detail::ObjectData* root_data)
    : session_(session), root_data_(root_data) {
}

void CommandExecuter::UpdateValue(CommandType type,
                                  const DObjPath& /*path*/,
                                  const std::string& key,
                                  const DValue& new_value,
                                  const DValue& prev_value) {
  auto edit_type = static_cast<CommandType>(
      static_cast<int>(type) & static_cast<int>(CommandType::kEditTypeMask));
  if (edit_type == CommandType::kAdd
      || edit_type == CommandType::kUpdate) {
    root_data_->ExecUpdateValue(key, new_value, prev_value);
  } else if (edit_type == CommandType::kDelete) {
    root_data_->ExecRemoveValue(key, prev_value);
  } else if (edit_type == CommandType::kAdd) {
    root_data_->ExecAddValue(key, new_value);
  }
}

void CommandExecuter::UpdateBaseObjectList(CommandType type,
                                           const DObjPath& /*path*/,
                                           const DObjectSp& base_obj) {
  auto edit_type = static_cast<CommandType>(
      static_cast<int>(type) & static_cast<int>(CommandType::kEditTypeMask));
  if (edit_type == CommandType::kAdd) {
    root_data_->ExecAddBase(base_obj);
  } else if (edit_type == CommandType::kDelete) {
    root_data_->ExecRemoveBase(base_obj);
  }
}

DObjectSp CommandExecuter::UpdateChildList(CommandType type,
                                           const DObjPath& /*path*/,
                                           const std::string& child_name,
                                           const std::string& obj_type,
                                           bool is_flattened) {
  auto edit_type = static_cast<CommandType>(
      static_cast<int>(type) & static_cast<int>(CommandType::kEditTypeMask));
  DObjectSp child;
  if (edit_type == CommandType::kAdd) {
    child = root_data_->ExecCreateChild(child_name, obj_type, is_flattened);
  } else if (edit_type == CommandType::kDelete) {
    root_data_->ExecRemoveChild(child_name);
  }
  return child;
}

DObjPath CommandExecuter::RootObjPath() const {
  return root_data_->Path();
}

}  // namespace core

}  // namespace dino
