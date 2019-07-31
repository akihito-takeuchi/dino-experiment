// Copyright (c) 2019 Akihito Takeuchi
// Distributed under the MIT License : http://opensource.org/licenses/MIT

#include "dino/core/objectfactory.h"

#include <map>

#include "dino/core/detail/objectdata.h"

namespace dino {

namespace core {

namespace {

ObjectFactory::CreateFunc CreateDObject = [](const DataWp& data) {
  return new DObject(data);
};

}  // namespace

class ObjectFactory::Impl {
 public:
  struct ObjectConstructInfo {
    CreateFunc func;
    ObjectFlatTypeConst flat_type;
  };
  Impl() = default;
  ~Impl() = default;
  std::map<std::string, ObjectConstructInfo> object_info_map;
  CreateFunc default_create_func = CreateDObject;
  bool use_default = true;
};

ObjectFactory::ObjectFactory()
    : impl_(std::make_unique<Impl>()) {
}

ObjectFactory::~ObjectFactory() = default;

bool ObjectFactory::Register(const std::string& type,
                             const CreateFunc& func,
                             ObjectFlatTypeConst flat_type) {
  impl_->object_info_map[type] = {func, flat_type};
  return true;
}

bool ObjectFactory::SetDefaultCreateFunc(const CreateFunc& func) {
  impl_->default_create_func = func;
  return true;
}

DObject* ObjectFactory::Create(const DataWp& data) const {
  auto type = data.lock()->Type();
  auto itr = impl_->object_info_map.find(type);
  if (itr != impl_->object_info_map.cend())
    return itr->second.func(data);
  else if (!impl_->use_default)
    BOOST_THROW_EXCEPTION(
        ObjectFactoryException(kErrObjectTypeNotRegistered)
        << ExpInfo1(type));
  return impl_->default_create_func(data);
}

ObjectFactory::ObjectFlatTypeConst ObjectFactory::FlatType(
    const std::string& type) const {
  auto itr = impl_->object_info_map.find(type);
  if (itr == impl_->object_info_map.cend())
    return ObjectFlatTypeConst::kSpecifyAtCreation;
  return itr->second.flat_type;
}

bool ObjectFactory::IsFlattenedObject(const std::string& type) const {
  return FlatType(type) == ObjectFlatTypeConst::kFlattened;
}

bool ObjectFactory::UpdateFlattenedFlag(
    const std::string& type, bool is_flattened) const {
  auto flat_type = ObjectFactory::FlatType(type);
  switch (flat_type) {
    case ObjectFlatTypeConst::kFlattened:
      is_flattened = true;
      break;
    case ObjectFlatTypeConst::kNotFlattened:
      is_flattened = false;
      break;
    case ObjectFlatTypeConst::kSpecifyAtCreation:
      break;
  }
  return is_flattened;
}

void ObjectFactory::EnableDefault() {
  impl_->use_default = true;
}

void ObjectFactory::DisableDefault() {
  impl_->use_default = false;
}

void ObjectFactory::Reset() {
  impl_->use_default = true;
  impl_->object_info_map.clear();
  impl_->default_create_func = CreateDObject;
}

std::once_flag ObjectFactory::init_flag_;
std::unique_ptr<ObjectFactory> ObjectFactory::instance_ = nullptr;

ObjectFactory& ObjectFactory::Instance() {
  std::call_once(init_flag_, Init);
  return *instance_;
}

void ObjectFactory::Init() {
  instance_ = std::unique_ptr<ObjectFactory>(new ObjectFactory);
}

}  // namespace core

}  // namespace dino

