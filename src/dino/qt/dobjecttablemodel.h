// Copyright (c) 2019 Akihito Takeuchi
// Distributed under the MIT License : http://opensource.org/licenses/MIT

#pragma once

#include <QAbstractTableModel>
#include <QString>
#include <functional>
#include <memory>

#include "dino/core/fwd.h"

namespace dino {

namespace qt {

enum class SourceTypeConst {
  kValue,
  kChildObject
};

class ColumnInfo {
 public:
  using GetDataFuncType =
      std::function<QVariant (const core::DObjectSp&, int)>;
  using SetValueFuncType =
      std::function<bool (const core::DObjectSp&, const QVariant&, int)>;
  using GetFlagsFuncType =
      std::function<Qt::ItemFlags (const core::DObjectSp&)>;
  ColumnInfo(const QString& col_name,
             SourceTypeConst source_type,
             const QString& source_name,
             const GetDataFuncType& get_data_func,
             const GetFlagsFuncType& get_flags_func = GetFlagsFuncType(),
             const SetValueFuncType& set_value_func = SetValueFuncType());
  QString ColumnName() const;
  SourceTypeConst SourceType() const;
  QString SourceName() const;
  QVariant GetData(const core::DObjectSp& obj,
                    int role = Qt::DisplayRole) const;
  Qt::ItemFlags GetFlags(const core::DObjectSp& obj) const;
  bool SetValue(const core::DObjectSp& obj,
                const QVariant& value,
                int role = Qt::EditRole) const;

 private:
  QString col_name_;
  SourceTypeConst source_type_;
  QString source_name_;
  GetDataFuncType get_data_func_;
  GetFlagsFuncType get_flags_func_;
  SetValueFuncType set_value_func_;
};

class DObjectTableModel : public QAbstractTableModel {
  Q_OBJECT
 public:
  DObjectTableModel(const core::DObjectSp& root_obj, QObject* parent = nullptr);
  virtual ~DObjectTableModel();
  void InsertColumns(int col, const QList<ColumnInfo>& col_info_list);
  void RemoveColumns(int first, int last);
  void RemoveColumn(const QString& name);
  virtual int rowCount(const QModelIndex& parent = QModelIndex()) const;
  virtual int columnCount(const QModelIndex& parent = QModelIndex()) const;
  virtual QVariant data(
     const QModelIndex& index, int role = Qt::DisplayRole) const;
  virtual Qt::ItemFlags flags(const QModelIndex& index) const;
  virtual bool setData(const QModelIndex& index, const QVariant& value,
                       int role = Qt::EditRole);

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace qt

}  // namespace dino
