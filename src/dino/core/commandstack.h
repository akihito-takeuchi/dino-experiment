// Copyright (c) 2019 Akihito Takeuchi
// Distributed under the MIT License : http://opensource.org/licenses/MIT

#pragma once

#include <deque>
#include <vector>
#include <string>
#include <utility>

#include "dino/core/commandexecuter.h"
#include "dino/core/callback.h"

namespace dino {

namespace core {

class CommandStack : public CommandExecuter {
 public:
  ~CommandStack() = default;
  void StartBatch(const std::string& description);
  void EndBatch();
  void CancelBatch();
  void Clear();
  void Clean();
  bool IsClean() const;
  bool CanRedo() const;
  void Redo();
  bool CanUndo() const;
  void Undo();
  void AddListener(const CommandStackListenerFunc& listener);
  
 private:
  CommandStack(Session* session, detail::ObjectData* root_data);
  // Create command
  virtual void UpdateValue(
      CommandType type,
      detail::ObjectData* data,
      const std::string& key,
      const DValue& new_value,
      const DValue& prev_value) override;
  virtual void UpdateBaseObjectList(
      CommandType type,
      detail::ObjectData* data,
      const DObjectSp& base_obj_name) override;
  virtual DObjectSp UpdateChildList(
      CommandType type,
      detail::ObjectData* data,
      const std::string& child_name,
      const std::string& obj_type,
      bool is_flattened,
      const PostCreateFunc& post_func) override;

  struct RemovedData;
  using RemovedDataSp = std::shared_ptr<RemovedData>;
  using CommandData = std::tuple<Command, RemovedDataSp, PostCreateFunc>;
  using BatchCommandData = std::vector<CommandData>;

  void PushCommand(const Command& cmd,
                   const PostCreateFunc& post_func = PostCreateFunc());
  void ExecRedo(CommandData& cmd_data);
  void ExecUndo(CommandData& cmd_data);
  void StoreChildData(detail::ObjectData* obj,
                      const std::string& child_name,
                      const RemovedDataSp& data);
  void RestoreChildData(detail::ObjectData* obj,
                        const RemovedDataSp& data,
                        bool emit_signal);
  void PutObjectData(detail::ObjectData* obj,
                     const RemovedDataSp& data);
  void PushBatchCommand(const std::string& description,
                        const BatchCommandData& batch_data,
                        bool emit_signal = true);

  std::deque<std::pair<std::string, BatchCommandData>> stack_;
  boost::signals2::signal<void()> sig_;
  std::string batch_description_;
  BatchCommandData batch_;
  size_t current_pos_ = 0;
  size_t clean_pos_ = 0;
  bool in_batch_ = false;
  bool in_command_ = false;
  friend class DObject;
  friend class detail::ObjectData;
};

}  // namespace core

}  // namespace dino
