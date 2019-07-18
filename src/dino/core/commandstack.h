// Copyright (c) 2019 Akihito Takeuchi
// Distributed under the MIT License : http://opensource.org/licenses/MIT

#pragma once

#include <deque>
#include <vector>
#include <string>
#include <utility>

#include "dino/core/commandexecuter.h"

namespace dino {

namespace core {

class CommandStack : public CommandExecuter {
 public:
  ~CommandStack() = default;
  void StartTransaction(const std::string& description);
  void EndTransaction();
  void CancelTransaction();
  void Clear();
  void Clean();
  bool IsClean() const;
  bool CanRedo() const;
  void Redo();
  bool CanUndo() const;
  void Undo();
  
 private:
  CommandStack(Session* session, detail::ObjectData* root_data);
  // Create command
  virtual void UpdateValue(
      CommandType type,
      const DObjPath& path,
      const std::string& key,
      const DValue& new_value,
      const DValue& prev_value) override;
  virtual void UpdateBaseObjectList(
      CommandType type,
      const DObjPath& path,
      const DObjectSp& base_obj_name) override;
  virtual DObjectSp UpdateChildList(
      CommandType type,
      const DObjPath& path,
      const std::string& child_name,
      const std::string& obj_type,
      bool is_flattened) override;

  struct RemovedData;
  using RemovedDataSp = std::shared_ptr<RemovedData>;
  using CommandData = std::pair<Command, RemovedDataSp>;

  void PushCommand(const Command& cmd);
  void ExecRedo(CommandData& cmd_data);
  void ExecUndo(CommandData& cmd_data);
  void StoreChildData(detail::ObjectData* obj,
                      const std::string& child_name,
                      const RemovedDataSp& data);
  void RestoreChildData(detail::ObjectData* obj,
                        const RemovedDataSp& data);

  std::deque<std::pair<std::string, std::vector<CommandData>>> stack_;
  std::string transaction_description_;
  std::vector<CommandData> transaction_;
  size_t current_pos_ = 0;
  size_t clean_pos_ = 0;
  bool in_transaction_ = false;
  friend class DObject;
  friend class detail::ObjectData;
};

}  // namespace core

}  // namespace dino
