// Copyright (c) 2018 Akihito Takeuchi
// Distributed under the MIT License : http://opensource.org/licenses/MIT

#include "dino/qt/dobjecttablemodel.h"

#include <QList>

#include "dino/core/dobject.h"

namespace dino {

namespace qt {

ColumnInfo::ColumnInfo(
    const QString& col_name,
    SourceTypeConst source_type,
    const QString& source_name,
    const GetDataFuncType& get_data_func,
    const GetFlagsFuncType& get_flags_func,
    const SetDataFuncType& set_data_func)
    : col_name_(col_name), source_type_(source_type),
      source_name_(source_name),
      get_data_func_(get_data_func),
      get_flags_func_(get_flags_func),
      set_data_func_(set_data_func) {
}

QString ColumnInfo::ColumnName() const {
  return col_name_;
}

SourceTypeConst ColumnInfo::SourceType() const {
  return source_type_;
}

QString ColumnInfo::SourceName() const {
  return source_name_;
}

QVariant ColumnInfo::GetData(const core::DObjectSp& obj, int role) const {
  return get_data_func_(obj, role);
}

Qt::ItemFlags ColumnInfo::GetFlags(const core::DObjectSp& obj) const {
  return get_flags_func_(obj);
}

bool ColumnInfo::SetData(
    const core::DObjectSp& obj, const QVariant& value, int role) const {
  return set_data_func_(obj, value, role);
}

class DObjectTableModel::Impl {
 public:
  Impl() = default;
  ~Impl() = default;
  core::DObjectSp root_obj;
  QList<ColumnInfo> col_info_list;
};

DObjectTableModel::DObjectTableModel(const core::DObjectSp& root_obj, QObject* parent)
    : QAbstractTableModel(parent), impl_(std::make_unique<Impl>()) {
  impl_->root_obj = root_obj;
}

DObjectTableModel::~DObjectTableModel() = default;

void DObjectTableModel::InsertColumns(
    int col, const QList<ColumnInfo>& col_info_list) {
  beginInsertColumns(QModelIndex(), col, col + col_info_list.size() - 1);
  for (auto& col_info : col_info_list)
    impl_->col_info_list.insert(col, col_info);
  endInsertColumns();
}

void DObjectTableModel::RemoveColumns(int first, int last) {
  beginRemoveColumns(QModelIndex(), first, last);
  while (last >= first) {
    impl_->col_info_list.removeAt(first);
    last --;
  }
  endRemoveColumns();
}

void DObjectTableModel::RemoveColumn(const QString& name) {
  auto itr = std::find_if(
      impl_->col_info_list.begin(),
      impl_->col_info_list.end(),
      [&name](auto& col_info) { return col_info.ColumnName() == name; });
  int col = std::distance(impl_->col_info_list.begin(), itr);
  RemoveColumns(col, col);
}

int DObjectTableModel::rowCount(const QModelIndex&) const {
  return impl_->root_obj->ChildCount();
}

int DObjectTableModel::columnCount(const QModelIndex&) const {
  return impl_->col_info_list.size();
}

QVariant DObjectTableModel::data(const QModelIndex& index, int role) const {
  auto obj = impl_->root_obj->GetChildObject(index.row());
  auto col_info = impl_->col_info_list[index.column()];
  return col_info.GetData(obj, role);
}

Qt::ItemFlags DObjectTableModel::flags(const QModelIndex& index) const {
  auto obj = impl_->root_obj->GetChildObject(index.row());
  auto col_info = impl_->col_info_list[index.column()];
  return col_info.GetFlags(obj);
}

bool DObjectTableModel::setData(
    const QModelIndex& index, const QVariant& value, int role) {
  auto obj = impl_->root_obj->GetChildObject(index.row());
  auto col_info = impl_->col_info_list[index.column()];
  return col_info.SetData(obj, value, role);
}

}  // namespace qt

}  // namespace dino
