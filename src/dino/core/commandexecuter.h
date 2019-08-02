// Copyright (c) 2019 Akihito Takeuchi
// Distributed under the MIT License : http://opensource.org/licenses/MIT

#pragma once

#include "dino/core/fwd.h"
#include "dino/core/command.h"
#include "dino/core/dobjpath.h"

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
      const DObjPath& path,
      const std::string& key,
      const DValue& new_value,
      const DValue& prev_value);
  virtual void UpdateBaseObjectList(
      CommandType type,
      const DObjPath& path,
      const DObjectSp& base_obj);
  virtual DObjectSp UpdateChildList(
      CommandType type,
      const DObjPath& path,
      const std::string& child_name,
      const std::string& obj_type,
      bool is_flattened);
  CommandExecuter(Session* session, detail::ObjectData* root_data);

  Session* session_;
  detail::ObjectData* root_data_;

  friend class detail::ObjectData;
};

}  // namespace core

}  // namespace dino
