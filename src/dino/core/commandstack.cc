// Copyright (c) 2019 Akihito Takeuchi
// Distributed under the MIT License : http://opensource.org/licenses/MIT

#include "dino/core/commandstack.h"

#include <boost/range/adaptor/reversed.hpp>

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

void CommandStack::StartBatch(const std::string& description) {
  if (in_batch_)
    BOOST_THROW_EXCEPTION(
        CommandStackException(kErrBatchCommandError)
        << ExpInfo1("has already started"));
  batch_description_ = description;
  batch_.clear();
  in_batch_ = true;
}

void CommandStack::EndBatch() {
  if (!in_batch_)
    BOOST_THROW_EXCEPTION(
        CommandStackException(kErrBatchCommandError)
        << ExpInfo1("has not started"));
  PushBatchCommand(batch_description_, batch_);
  in_batch_ = false;
}

void CommandStack::CancelBatch() {
  if (!in_batch_)
    BOOST_THROW_EXCEPTION(
        CommandStackException(kErrBatchCommandError)
        << ExpInfo1("has not started"));
  // put each command in batch as single command to stack
  for (auto& command_data : batch_)
    PushBatchCommand("", {command_data}, false);
  in_batch_ = false;
  sig_();
}

void CommandStack::Clear() {
  stack_.clear();
  current_pos_ = clean_pos_ = 0;
  sig_();
}

void CommandStack::Clean() {
  clean_pos_ = current_pos_;
  sig_();
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
  in_command_ = true;
  for (auto& cmd : stack_[current_pos_].second)
    ExecRedo(cmd);
  current_pos_ ++;
  in_command_ = false;
  sig_();
}

bool CommandStack::CanUndo() const {
  return current_pos_ > 0;
}

void CommandStack::Undo() {
  if (!CanUndo())
    BOOST_THROW_EXCEPTION(
        CommandStackException(kErrNoUndoEntry)
        << ExpInfo1(RootObjPath().String()));
  in_command_ = true;
  for (auto& cmd : stack_[current_pos_-1].second | boost::adaptors::reversed)
    ExecUndo(cmd);
  current_pos_ --;
  in_command_ = false;
  sig_();
}

void CommandStack::AddListener(const CommandStackListenerFunc& listener) {
  sig_.connect(listener);
}

void CommandStack::PushCommand(const Command& cmd) {
  CommandData cmd_data(cmd, nullptr);
  ExecRedo(cmd_data);
  if (in_batch_)
    batch_.push_back(cmd_data);
  else
    PushBatchCommand("", {cmd_data});
}

void CommandStack::PushBatchCommand(const std::string& description,
                                    const BatchCommandData& batch_data,
                                    bool emit_signal) {
  stack_.resize(current_pos_);
  stack_.emplace_back(description, batch_data);
  current_pos_ ++;
  if (emit_signal)
    sig_();
}

void CommandStack::UpdateValue(CommandType type,
                               detail::ObjectData* data,
                               const std::string& key,
                               const DValue& new_value,
                               const DValue& prev_value) {
  if (in_command_) {
    CommandExecuter::UpdateValue(type, data, key, new_value, prev_value);
    return;
  }
  in_command_ = true;
  CommandType cmd_type = static_cast<CommandType>(
      static_cast<int>(type)
      | static_cast<int>(CommandType::kValueUpdateType));
  Command cmd(cmd_type, data->Path(), key, new_value, prev_value,
              DObjPath(), "", {});
  PushCommand(cmd);
  in_command_ = false;
}

void CommandStack::UpdateBaseObjectList(CommandType type,
                                        detail::ObjectData* data,
                                        const DObjectSp& base_obj) {
  if (in_command_) {
    CommandExecuter::UpdateBaseObjectList(type, data, base_obj);
    return;
  }
  in_command_ = true;
  CommandType cmd_type = static_cast<CommandType>(
      static_cast<int>(type)
      | static_cast<int>(CommandType::kBaseObjectUpdateType));
  Command cmd(cmd_type, data->Path(), "", nil, nil, base_obj->Path(), base_obj->Type(),
              data->Children());
  PushCommand(cmd);
  in_command_ = false;
}

DObjectSp CommandStack::UpdateChildList(CommandType type,
                                        detail::ObjectData* data,
                                        const std::string& child_name,
                                        const std::string& obj_type,
                                        bool is_flattened) {
  if (in_command_)
    return CommandExecuter::UpdateChildList(
        type, data, child_name, obj_type, is_flattened);
  in_command_ = true;
  CommandType cmd_type = static_cast<CommandType>(
      static_cast<int>(type)
      | static_cast<int>(CommandType::kChildListUpdateType));
  if (is_flattened && type == CommandType::kAddChild)
    type = CommandType::kAddFlattenedChild;
  auto path = data->Path();
  Command cmd(
      cmd_type, path, "", nil, nil, child_name, obj_type, data->Children());
  PushCommand(cmd);

  DObjectSp child;
  if (type == CommandType::kAdd) {
    child = session_->GetObject(path.ChildPath(child_name));
    child->SetEditable();
  }
  in_command_ = false;
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
    case CommandType::kDeleteChild:
      if (!cmd_data.second) {
        cmd_data.second = std::make_shared<RemovedData>();
        StoreChildData(obj, cmd.TargetObjectName(), cmd_data.second);
      }
      obj->ExecDeleteChild(cmd.TargetObjectName());
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
      obj->ExecDeleteChild(cmd.TargetObjectName());
      break;
    case CommandType::kDeleteChild:
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
