// Copyright (c) 2018 Akihito Takeuchi
// Distributed under the MIT License : http://opensource.org/licenses/MIT

#include "dino/core/dobject.h"

#include <unordered_map>
#include <boost/filesystem.hpp>

#include "dino/core/detail/objectdata.h"
#include "dino/core/detail/objectdataexception.h"
#include "dino/core/dobjectexception.h"

namespace fs = boost::filesystem;

#define REQUIRE_EDITABLE()                              \
  if (!IsEditable())                                    \
    BOOST_THROW_EXCEPTION(                              \
        DObjectException(kErrObjectIsNotEditable)       \
        << ExpInfo1(Name()));

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

void DObject::Put(const std::string& key, const char* str_value) {
  Put(key, DValue(std::string(str_value)));
}

void DObject::Put(const std::string& key, const DValue& value) {
  REQUIRE_EDITABLE();
  impl_->GetRawData()->Put(key, value);
}

void DObject::RemoveKey(const std::string& key) {
  REQUIRE_EDITABLE();
  impl_->GetRawData()->RemoveKey(key);
}

bool DObject::IsLocalKey(const std::string& key) const {
  return impl_->GetRawData()->IsLocalKey(key);
}

bool DObject::HasNonLocalKey(const std::string& key) const {
  return impl_->GetRawData()->HasNonLocalKey(key);
}

DObjPath DObject::WhereIsKey(const std::string& key) const {
  return impl_->GetRawData()->WhereIsKey(key);
}

std::vector<std::string> DObject::Keys(bool local_only) const {
  return impl_->GetRawData()->Keys(local_only);
}

bool DObject::HasAttr(const std::string& key) const {
  return impl_->GetRawData()->HasAttr(key);
}

std::string DObject::Attr(const std::string& key) const {
  return impl_->GetRawData()->Attr(key);
}

std::map<std::string, std::string> DObject::Attrs() const {
  return impl_->GetRawData()->Attrs();
}

void DObject::SetTemporaryAttr(const std::string& key,
                               const std::string& value) {
  impl_->GetRawData()->SetTemporaryAttr(key, value);
}

void DObject::SetAttr(const std::string& key, const std::string& value) {
  REQUIRE_EDITABLE();
  impl_->GetRawData()->SetAttr(key, value);
}

void DObject::RemoveAttr(const std::string& key) {
  if (!HasAttr(key))
    BOOST_THROW_EXCEPTION(
        DObjectException(detail::kErrAttrDoesNotExist)
        << ExpInfo1(Path().String()) << ExpInfo2(key));

  if (HasPersistentAttr(key))
    REQUIRE_EDITABLE();
  impl_->GetRawData()->RemoveAttr(key);
}

bool DObject::IsTemporaryAttr(const std::string& key) const {
  return impl_->GetRawData()->IsTemporaryAttr(key);
}

bool DObject::HasPersistentAttr(const std::string& key) const {
  return impl_->GetRawData()->HasPersistentAttr(key);
}

void DObject::SetAllAttrsToBeSaved() {
  REQUIRE_EDITABLE();
  impl_->GetRawData()->SetAllAttrsToBeSaved();
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

bool DObject::IsActual() const {
  return impl_->GetRawData()->IsActual();
}

bool DObject::HasActualChild(const std::string& name) const {
  return impl_->GetRawData()->HasActualChild(name);
}

bool DObject::IsActualChild(const std::string& name) const {
  return impl_->GetRawData()->IsActualChild(name);
}

bool DObject::IsChildOpened(const std::string& name) const {
  return impl_->GetRawData()->IsChildOpened(name);
}

std::vector<DObjInfo> DObject::Children() const {
  return impl_->GetRawData()->Children();
}

DObjInfo DObject::ChildInfo(const std::string& name) const {
  return impl_->GetRawData()->ChildInfo(name);
}

size_t DObject::ChildCount() const {
  return impl_->GetRawData()->ChildCount();
}

DObjectSp DObject::GetChildAt(size_t index, OpenMode mode) const {
  auto raw_data = impl_->GetRawData();
  if (index > raw_data->ChildCount())
    BOOST_THROW_EXCEPTION(
        DObjectException(kErrChildIndexOutOfRange)
        << ExpInfo1(index) << ExpInfo2(Path().String()));
  return raw_data->OpenChild(raw_data->Children()[index].Name(), mode);
}

DObjectSp DObject::OpenChild(const std::string& name, OpenMode mode) const {
  return impl_->GetRawData()->OpenChild(name, mode);
}

DObjectSp DObject::CreateChild(const std::string& name,
                               const std::string& type,
                               bool is_flattened) {
  REQUIRE_EDITABLE();
  return impl_->GetRawData()->CreateChild(name, type, is_flattened);
}

DObjectSp DObject::Parent() const {
  return impl_->GetRawData()->Parent();
}

uintptr_t DObject::ObjectId() const {
  return impl_->GetRawData()->ObjectId();
}

void DObject::RefreshChildren() {
  impl_->GetRawData()->RefreshChildren();
}

void DObject::SortChildren() {
  impl_->GetRawData()->SortChildren();
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

void DObject::DeleteChild(const std::string& name) {
  REQUIRE_EDITABLE();
  impl_->GetRawData()->DeleteChild(name);
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
  REQUIRE_EDITABLE();
  impl_->GetRawData()->SetDirty(dirty);
}

void DObject::AddBase(const DObjectSp& base) {
  REQUIRE_EDITABLE();
  impl_->GetRawData()->AddBase(base);
}

std::vector<DObjectSp> DObject::Bases() const {
  return impl_->GetRawData()->Bases();
}

void DObject::RemoveBase(const DObjectSp& base) {
  REQUIRE_EDITABLE();
  return impl_->GetRawData()->RemoveBase(base);
}

std::vector<DObjectSp> DObject::BasesFromParent() const {
  return impl_->GetRawData()->BasesFromParent();
}

std::vector<DObjectSp> DObject::EffectiveBases() const {
  return impl_->GetRawData()->EffectiveBases();
}

boost::signals2::connection DObject::AddListener(
    const ObjectListenerFunc& listener, ListenerCallPoint call_point) {
  return impl_->GetRawData()->AddListener(listener, call_point);
}

void DObject::DisableSignal() {
  impl_->GetRawData()->DisableSignal();
}

void DObject::EnableSignal() {
  impl_->GetRawData()->EnableSignal();
}

CommandStackSp DObject::EnableCommandStack(bool enable) {
  return impl_->GetRawData()->EnableCommandStack(enable);
}

CommandStackSp DObject::GetCommandStack() const {
  return impl_->GetRawData()->GetCommandStack();
}

void DObject::Save(bool recurse) {
  REQUIRE_EDITABLE();
  PreSaveHook();
  impl_->GetRawData()->Save(recurse);
}

// Do nothing. This will be implemented in the derived class
void DObject::PreSaveHook() {
}

SessionPtr DObject::GetSession() {
  return impl_->GetRawData()->GetSession();
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
