// Copyright (c) 2019 Akihito Takeuchi
// Distributed under the MIT License : http://opensource.org/licenses/MIT

#pragma once

#include "dino/core/fwd.h"
#include "dino/core/command.h"
#include "dino/core/dobjpath.h"
#include "dino/core/callback.h"

namespace dino {

namespace core {

class Session;

namespace detail {

class ObjectData;

};

class CommandExecuter {
 public:
  virtual ~CommandExecuter() = default;
  DObjPath RootObjPath() const;
 protected:
  // Create command
  virtual void UpdateValue(
      CommandType type,
      detail::ObjectData* data,
      const std::string& key,
      const DValue& new_value,
      const DValue& prev_value);
  virtual void UpdateBaseObjectList(
      CommandType type,
      detail::ObjectData* data,
      const DObjectSp& base_obj);
  virtual DObjectSp UpdateChildList(
      CommandType type,
      detail::ObjectData* data,
      const std::string& child_name,
      const std::string& obj_type,
      bool is_flattened,
      const PostCreateFunc& post_func);
  CommandExecuter(Session* session, detail::ObjectData* root_data);

  Session* session_;
  detail::ObjectData* root_data_;

  friend class detail::ObjectData;
};

}  // namespace core

}  // namespace dino
