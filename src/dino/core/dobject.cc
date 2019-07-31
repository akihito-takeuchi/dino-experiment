// Copyright (c) 2018 Akihito Takeuchi
// Distributed under the MIT License : http://opensource.org/licenses/MIT

#include "dino/core/dobject.h"

#include <unordered_map>
#include <boost/filesystem.hpp>

#include "dino/core/detail/objectdata.h"
#include "dino/core/dobjectexception.h"

namespace fs = boost::filesystem;

namespace dino {

namespace core {

struct DObject::Impl {
  Impl(const DataWp& data) :
      data_(data),
      raw_data_(data.lock().get()) {
    raw_data_->IncRef();
    path_ = raw_data_->Path();
  }
  ~Impl() {
    if (!data_.expired())
      raw_data_->DecRef(editable_);
  }
  std::string Name() const;
  DObjPath Path() const;
  detail::ObjectData* GetRawData();
  bool IsEditable() const;
  void SetEditable();
  void SetReadOnly();
  bool IsExpired() const;
  void RequireEditable();

 private:
  DObjPath path_;
  DataWp data_;
  bool editable_ = false;
  detail::ObjectData* raw_data_;
};

DObjPath DObject::Impl::Path() const {
  return path_;
}

std::string DObject::Impl::Name() const {
  return path_.LeafName();
}

detail::ObjectData* DObject::Impl::GetRawData() {
  if (data_.expired())
    BOOST_THROW_EXCEPTION(
        DObjectException(kErrObjectExpired)
        << ExpInfo1(path_.String()));
  return raw_data_;
}

bool DObject::Impl::IsEditable() const {
  return editable_;
}

void DObject::Impl::SetEditable() {
  editable_ = true;
}

void DObject::Impl::SetReadOnly() {
  editable_ = false;
}

bool DObject::Impl::IsExpired() const {
  return data_.expired();
}

void DObject::Impl::RequireEditable() {
  if (!IsEditable())
    BOOST_THROW_EXCEPTION(
        DObjectException(kErrObjectIsNotEditable)
        << ExpInfo1(Name()));
}

DObject::DObject(const DataWp& data)
    : impl_(std::make_unique<Impl>(data)) {
}

DObject::~DObject() = default;

bool DObject::HasKey(const std::string& key) const {
  return impl_->GetRawData()->HasKey(key);
}

DValue DObject::Get(const std::string& key, const DValue& default_value) const {
  return impl_->GetRawData()->Get(key, default_value);
}

DValue DObject::Get(const std::string& key) const {
  return impl_->GetRawData()->Get(key);
}

void DObject::Put(const std::string& key, const DValue& value) {
  impl_->RequireEditable();
  impl_->GetRawData()->Put(key, value);
}

void DObject::RemoveKey(const std::string& key) {
  impl_->RequireEditable();
  impl_->GetRawData()->RemoveKey(key);
}

bool DObject::IsLocal(const std::string& key) const {
  return impl_->GetRawData()->IsLocal(key);
}

DObjPath DObject::Where(const std::string& key) const {
  return impl_->GetRawData()->Where(key);
}

std::vector<std::string> DObject::Keys(bool local_only) const {
  return impl_->GetRawData()->Keys(local_only);
}


std::string DObject::Name() const {
  return impl_->Name();
}

std::string DObject::Type() const {
  return impl_->GetRawData()->Type();
}

FsPath DObject::DirPath() const {
  return impl_->GetRawData()->DirPath();
}

DObjPath DObject::Path() const {
  return impl_->Path();
}

bool DObject::HasChild(const std::string& name) const {
  return impl_->GetRawData()->HasChild(name);
}

bool DObject::HasLocalChild(const std::string& name) const {
  return impl_->GetRawData()->HasLocalChild(name);
}

bool DObject::IsLocalChild(const std::string& name) const {
  return impl_->GetRawData()->IsLocalChild(name);
}

bool DObject::IsChildOpened(const std::string& name) const {
  return impl_->GetRawData()->IsChildOpened(name);
}

std::vector<DObjInfo> DObject::Children() const {
  return impl_->GetRawData()->Children();
}

size_t DObject::ChildCount() const {
  return impl_->GetRawData()->ChildCount();
}

DObjectSp DObject::GetChildObject(size_t index) const {
  return impl_->GetRawData()->GetChildObject(index);
}

DObjectSp DObject::GetChildObject(const std::string& name) const {
  return impl_->GetRawData()->GetChildObject(name);
}

DObjectSp DObject::OpenChildObject(const std::string& name) const {
  return impl_->GetRawData()->OpenChildObject(name);
}

DObjectSp DObject::CreateChild(const std::string& name,
                               const std::string& type,
                               bool is_flattened) {
  return impl_->GetRawData()->CreateChild(name, type, is_flattened);
}

DObjectSp DObject::Parent() const {
  return impl_->GetRawData()->Parent();
}

uintptr_t DObject::ObjectID() const {
  return impl_->GetRawData()->ObjectID();
}

void DObject::RefreshChildren() {
  impl_->GetRawData()->RefreshChildren();
}

bool DObject::IsFlattened() const {
  return impl_->GetRawData()->IsFlattened();
}

bool DObject::IsChildFlat(const std::string& name) const {
  return impl_->GetRawData()->IsChildFlat(name);
}

void DObject::SetChildFlat(const std::string& name) {
  impl_->GetRawData()->SetChildFlat(name);
}

void DObject::UnsetChildFlat(const std::string& name) {
  impl_->GetRawData()->UnsetChildFlat(name);
}

bool DObject::IsEditable() const {
  return impl_->IsEditable();
}

bool DObject::IsReadOnly() const {
  return !impl_->IsEditable();
}

void DObject::SetEditable() {
  if (impl_->IsEditable())
    return;
  impl_->GetRawData()->AcquireWriteLock();
  impl_->SetEditable();
}

void DObject::SetReadOnly() {
  if (!impl_->IsEditable())
    return;
  impl_->GetRawData()->ReleaseWriteLock();
  impl_->SetReadOnly();
}

bool DObject::IsExpired() const {
  return impl_->IsExpired();
}

bool DObject::IsDirty() const {
  return impl_->GetRawData()->IsDirty();
}

void DObject::SetDirty(bool dirty) {
  impl_->GetRawData()->SetDirty(dirty);
}

void DObject::AddBase(const DObjectSp& base) {
  impl_->RequireEditable();
  impl_->GetRawData()->AddBase(base);
}

std::vector<DObjectSp> DObject::BaseObjects() const {
  return impl_->GetRawData()->BaseObjects();
}

void DObject::RemoveBase(const DObjectSp& base) {
  return impl_->GetRawData()->RemoveBase(base);
}

boost::signals2::connection DObject::AddListener(
    const ObjectListenerFunc& listener) {
  return impl_->GetRawData()->AddListener(listener);
}

CommandStackSp DObject::EnableCommandStack(bool enable) {
  return impl_->GetRawData()->EnableCommandStack(enable);
}

CommandStackSp DObject::GetCommandStack() const {
  return impl_->GetRawData()->GetCommandStack();
}

void DObject::Save() {
  if (!impl_->IsEditable())
    BOOST_THROW_EXCEPTION(
        DObjectException(kErrObjectIsNotEditable)
        << ExpInfo1(Name()));
  impl_->GetRawData()->Save();
}

bool DObject::IsObjectDir(const FsPath& path) {
  auto file_info = detail::ObjectData::GetFileInfo(path);
  return file_info.IsValid();
}

detail::ObjectData* DObject::GetData() {
  return impl_->GetRawData();
}

}  // namespace core

}  // namespace dino
