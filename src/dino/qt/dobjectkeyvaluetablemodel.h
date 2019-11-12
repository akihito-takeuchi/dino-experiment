// Copyright (c) 2019 Akihito Takeuchi
// Distributed under the MIT License : http://opensource.org/licenses/MIT

#pragma once

#include <QAbstractTableModel>

#include <memory>
#include <QBrush>
#include <QFont>

#include "dino/core/dvalue.h"
#include "dino/core/fwd.h"

namespace dino {

namespace qt {

class DObjectKeyValueTableModel : public QAbstractTableModel {
  Q_OBJECT
 public:
  enum class ColumnType {
    kKey,
    kValue,
    kType
  };
  struct CellStyle {
    QBrush foreground_color;
    QBrush background_color;
    QFont font;
  };
  using DValueToStringFunc = std::function<QString (const core::DValue& value)>;
  using StringToDValueFunc = std::function<core::DValue (const QString& value_str)>;
  using GetStyleFunc = std::function<CellStyle (const std::string& key)>;
  DObjectKeyValueTableModel(const core::DObjectSp& root_obj,
                            const core::DObjectSp& listen_to = nullptr,
                            QObject* parent = nullptr);
  virtual ~DObjectKeyValueTableModel();

  bool ShowValueType() const;
  void SetShowValueType(bool show);

  virtual QVariant headerData(
      int section, Qt::Orientation orientation, int role) const override;
  virtual int columnCount(const QModelIndex& parent) const override;
  virtual int rowCount(const QModelIndex& parent) const override;
  virtual QVariant data(const QModelIndex& index,
                        int role = Qt::DisplayRole) const override;
  virtual Qt::ItemFlags flags(const QModelIndex& index) const override;
  virtual bool setData(const QModelIndex& index, const QVariant& value,
                       int role = Qt::EditRole) override;

  void SetConversionFunc(const DValueToStringFunc& value_to_string_func,
                         const StringToDValueFunc& string_to_value_func);
  void SetStyleFunc(const GetStyleFunc& key_col_style_func,
                    const GetStyleFunc& value_col_style_func,
                    const GetStyleFunc& type_col_style_func = GetStyleFunc());

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace qt

}  // namespace dino
