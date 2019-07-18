// Copyright (c) 2019 Akihito Takeuchi
// Distributed under the MIT License : http://opensource.org/licenses/MIT

#include "dino/core/commandstack.h"
#include "dino/core/commandstackexception.h"
#include "dino/core/session.h"
#include "dino/core/dobject.h"
#include "dino/core/detail/objectdata.h"

namespace dino {

namespace core {

struct CommandStack::RemovedData {
  RemovedData() {}
  std::string name;
  std::string type;
  DValueDict values;
  std::vector<RemovedDataSp> children;
  std::vector<DObjPath> base_objects;
  bool is_flattened;
};

CommandStack::CommandStack(Session* session, detail::ObjectData* root_data)
    : CommandExecuter(session, root_data) {
}

void CommandStack::StartTransaction(const std::string& description) {
  if (in_transaction_)
    BOOST_THROW_EXCEPTION(
        CommandStackException(kErrTransactionError)
        << ExpInfo1("has already started"));
  transaction_description_ = description;
  transaction_.clear();
  in_transaction_ = true;
}

void CommandStack::EndTransaction() {
  if (!in_transaction_)
    BOOST_THROW_EXCEPTION(
        CommandStackException(kErrTransactionError)
        << ExpInfo1("has not started"));
  stack_.emplace_back(transaction_description_, transaction_);
  in_transaction_ = false;
  Redo();
}

void CommandStack::CancelTransaction() {
  if (!in_transaction_)
    BOOST_THROW_EXCEPTION(
        CommandStackException(kErrTransactionError)
        << ExpInfo1("has not started"));
  in_transaction_ = false;
}

void CommandStack::Clear() {
  stack_.clear();
  current_pos_ = clean_pos_ = 0;
}

void CommandStack::Clean() {
  clean_pos_ = current_pos_;
}

bool CommandStack::IsClean() const {
  return current_pos_ == clean_pos_;
}

bool CommandStack::CanRedo() const {
  return current_pos_ < stack_.size();
}

void CommandStack::Redo() {
  if (!CanRedo())
    BOOST_THROW_EXCEPTION(
        CommandStackException(kErrNoRedoEntry)
        << ExpInfo1(RootObjPath().String()));
  for (auto& cmd : stack_[current_pos_].second)
    ExecRedo(cmd);
  current_pos_ ++;
}

bool CommandStack::CanUndo() const {
  return current_pos_ > 0;
}

void CommandStack::Undo() {
  if (!CanUndo())
    BOOST_THROW_EXCEPTION(
        CommandStackException(kErrNoUndoEntry)
        << ExpInfo1(RootObjPath().String()));
  for (auto& cmd : stack_[current_pos_-1].second)
    ExecUndo(cmd);
  current_pos_ --;
}

void CommandStack::PushCommand(const Command& cmd) {
  CommandData cmd_data(cmd, nullptr);
  if (in_transaction_) {
    transaction_.push_back(cmd_data);
  } else {
    stack_.emplace_back("", std::vector<CommandData>({cmd_data}));
    Redo();
  }
}

void CommandStack::UpdateValue(CommandType type,
                               const DObjPath& path,
                               const std::string& key,
                               const DValue& new_value,
                               const DValue& prev_value) {
  CommandType cmd_type = static_cast<CommandType>(
      static_cast<int>(type)
      | static_cast<int>(CommandType::kValueUpdateType));
  Command cmd(cmd_type, path, key, new_value, prev_value,
              DObjPath(), "", {});
  PushCommand(cmd);
}

void CommandStack::UpdateBaseObjectList(CommandType type,
                                        const DObjPath& path,
                                        const DObjectSp& base_obj) {
  CommandType cmd_type = static_cast<CommandType>(
      static_cast<int>(type)
      | static_cast<int>(CommandType::kBaseObjectUpdateType));
  Command cmd(cmd_type, path, "", nil, nil, base_obj->Path(), base_obj->Type(),
              session_->GetObject(path)->Children());
  PushCommand(cmd);
}

DObjectSp CommandStack::UpdateChildList(CommandType type,
                                        const DObjPath& path,
                                        const std::string& child_name,
                                        const std::string& obj_type,
                                        bool is_flattened) {
  CommandType cmd_type = static_cast<CommandType>(
      static_cast<int>(type)
      | static_cast<int>(CommandType::kChildListUpdateType));
  if (is_flattened && type == CommandType::kAddChild)
    type = CommandType::kAddFlattenedChild;
  Command cmd(cmd_type, path, "", nil, nil, child_name, obj_type,
              session_->GetObject(path)->Children());
  PushCommand(cmd);
  DObjectSp child;
  auto edit_type = static_cast<CommandType>(
      static_cast<int>(type) & static_cast<int>(CommandType::kEditTypeMask));
  if (!in_transaction_ && edit_type == CommandType::kAdd) {
    child = session_->GetObject(path.ChildPath(child_name));
    child->SetEditable();
  }
  return child;
}

void CommandStack::ExecRedo(CommandData& cmd_data) {
  auto cmd = cmd_data.first;
  auto obj = root_data_->GetDataAt(cmd.ObjPath());
  switch (cmd.Type()) {
    case CommandType::kValueAdd:
      obj->ExecAddValue(cmd.Key(), cmd.NewValue());
      break;
    case CommandType::kValueUpdate:
      obj->ExecUpdateValue(cmd.Key(), cmd.NewValue(), cmd.PrevValue());
      break;
    case CommandType::kValueDelete:
      obj->ExecRemoveValue(cmd.Key(), cmd.PrevValue());
      break;
    case CommandType::kAddBaseObject:
      obj->ExecAddBase(session_->GetObject(cmd.TargetObjectPath()));
      break;
    case CommandType::kRemoveBaseObject:
      obj->ExecRemoveBase(session_->GetObject(cmd.TargetObjectPath()));
      break;
    case CommandType::kAddChild:
      obj->ExecCreateChild(cmd.TargetObjectName(),
                           cmd.TargetObjectType(),
                           false);
      break;
    case CommandType::kAddFlattenedChild:
      obj->ExecCreateChild(cmd.TargetObjectName(),
                           cmd.TargetObjectType(),
                           true);
      break;
    case CommandType::kRemoveChild:
      if (!cmd_data.second) {
        cmd_data.second = std::make_shared<RemovedData>();
        StoreChildData(obj, cmd.TargetObjectName(), cmd_data.second);
      }
      obj->ExecRemoveChild(cmd.TargetObjectName());
      break;
    default:
      BOOST_THROW_EXCEPTION(
          CommandStackException(kErrInvalidCommandTypeError)
          << ExpInfo1(static_cast<int>(cmd.Type())));
      break;
  }
}

void CommandStack::ExecUndo(CommandData& cmd_data) {
  auto cmd = cmd_data.first;
  auto obj = root_data_->GetDataAt(cmd.ObjPath());
  switch (cmd.Type()) {
    case CommandType::kValueAdd:
      obj->ExecRemoveValue(cmd.Key(), cmd.NewValue());
      break;
    case CommandType::kValueUpdate:
      obj->ExecUpdateValue(cmd.Key(), cmd.PrevValue(), cmd.NewValue());
      break;
    case CommandType::kValueDelete:
      obj->ExecAddValue(cmd.Key(), cmd.PrevValue());
      break;
    case CommandType::kAddBaseObject:
      obj->ExecRemoveBase(session_->GetObject(cmd.TargetObjectPath()));
      break;
    case CommandType::kRemoveBaseObject:
      obj->ExecAddBase(session_->GetObject(cmd.TargetObjectPath()));
      break;
    case CommandType::kAddChild:
    case CommandType::kAddFlattenedChild:
      obj->ExecRemoveChild(cmd.TargetObjectName());
      break;
    case CommandType::kRemoveChild:
      RestoreChildData(obj, cmd_data.second);
      break;
    default:
      BOOST_THROW_EXCEPTION(
          CommandStackException(kErrInvalidCommandTypeError)
          << ExpInfo1(static_cast<int>(cmd.Type())));
      break;
  }
}

void CommandStack::StoreChildData(detail::ObjectData* obj,
                                  const std::string& target_obj_name,
                                  const RemovedDataSp& data) {
  auto target = obj->GetChildObject(target_obj_name);
  data->name = target_obj_name;
  data->type = target->Type();
  data->is_flattened = target->IsFlattened();
  for (auto& base : target->BaseObjects())
    data->base_objects.push_back(base->Path());
  for (auto& key : target->Keys(true))
    data->values[key] = target->Get(key);

  auto children = target->Children();
  data->children.reserve(children.size());
  for (auto& child : children) {
    auto child_data = std::make_shared<RemovedData>();
    StoreChildData(root_data_->GetDataAt(child.Path()),
                   child.Name(), child_data);
  }
}

void CommandStack::RestoreChildData(detail::ObjectData* obj,
                                    const RemovedDataSp& data) {
  obj->ExecCreateChild(data->name, data->type, data->is_flattened);
  auto target_obj = root_data_->GetDataAt(obj->Path().ChildPath(data->name));
  for (auto& kv : data->values)
    target_obj->Put(kv.first, kv.second);
  for (auto& base_path : data->base_objects)
    target_obj->ExecAddBase(session_->GetObject(base_path));
  for (auto& child_data : data->children)
    RestoreChildData(target_obj, child_data);
}

}  // namespace core

}  // namespace dino
