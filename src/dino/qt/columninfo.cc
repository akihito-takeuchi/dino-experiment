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

ColumnInfo::~ColumnInfo() = default;

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

}  // namespace qt

}  // namespace dino
