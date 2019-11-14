// Copyright (c) 2019 Akihito Takeuchi
// Distributed under the MIT License : http://opensource.org/licenses/MIT

#pragma once

#include <QAbstractTableModel>

#include <memory>
#include <functional>
#include <QBrush>
#include <QFont>

#include "dino/core/dvalue.h"
#include "dino/core/fwd.h"

namespace dino {

namespace qt {

class DObjectKeyValueTableModel : public QAbstractTableModel {
  Q_OBJECT
 public:
  enum ColumnID {
    kKeyColumnID = 0,
    kValueColumnID = 1,
    kValueTypeColumnID = 2,
    kUserColumnID = 3
  };
  struct CellStyle {
    QBrush foreground_color;
    QBrush background_color;
    QFont font;
  };
  using DValueToStringFuncType =
      std::function<QString (const core::DValue& value)>;
  using StringToDValueFuncType =
      std::function<core::DValue (const QString& value_str)>;
  using GetStyleFuncType =
      std::function<CellStyle (const std::string& key)>;
  using GetDataFuncType =
      std::function<QString (const std::string& key)>;
  DObjectKeyValueTableModel(const core::DObjectSp& root_obj,
                            const core::DObjectSp& listen_to = nullptr,
                            QObject* parent = nullptr);
  virtual ~DObjectKeyValueTableModel();

  virtual QVariant headerData(
      int section, Qt::Orientation orientation, int role) const override;
  virtual int columnCount(const QModelIndex& parent) const override;
  virtual int rowCount(const QModelIndex& parent) const override;
  virtual QVariant data(const QModelIndex& index,
                        int role = Qt::DisplayRole) const override;
  virtual Qt::ItemFlags flags(const QModelIndex& index) const override;
  virtual bool setData(const QModelIndex& index, const QVariant& value,
                       int role = Qt::EditRole) override;

  // Returns ColumnID, the new column will not be displayed.
  // Need to call "InsertColumns"
  int CreateUserColumn(const QString& name,
                       const GetDataFuncType& get_data_func);
  QList<int> DisplayedColumnIDs() const;
  void InsertColumnIDs(int pos, const QList<int>& column_ids);
  void RemoveColumnIDs(const QList<int>& column_ids);
  void SetColumnName(int column_id, const QString& name);
  void SetConversionFunc(const DValueToStringFuncType& value_to_string_func,
                         const StringToDValueFuncType& string_to_value_func);
  void SetStyleFunc(int column_id, const GetStyleFuncType& get_style_func);

  bool IsEditable() const;
  void SetEditable(bool editable);

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace qt

}  // namespace dino
