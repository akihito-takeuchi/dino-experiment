// Copyright (c) 2019 Akihito Takeuchi
// Distributed under the MIT License : http://opensource.org/licenses/MIT

#pragma once

#include <QAbstractItemModel>
#include <memory>

#include "dino/core/fwd.h"
#include "dino/qt/columninfo.h"

namespace dino {

namespace qt {

class DObjectTreeModel : public QAbstractItemModel {
  Q_OBJECT
 public:
  DObjectTreeModel(const core::DObjectSp& root_obj,
                   QObject* parent = nullptr);
  virtual ~DObjectTreeModel();

  void InsertColumn(int col, const QList<ColumnInfo>& col_info_list);
  void RemoveColumns(int first, int last);

  virtual QVariant headerData(
      int section, Qt::Orientation orientation, int role) const override;
  virtual int columnCount(const QModelIndex& parent) const override;
  virtual int rowCount(const QModelIndex& parent) const override;
  virtual QVariant data(const QModelIndex& index,
                        int role = Qt::DisplayRole) const override;
  virtual Qt::ItemFlags flags(const QModelIndex& index) const override;
  virtual bool setData(const QModelIndex& index, const QVariant& value,
                       int role = Qt::EditRole) override;
  virtual QModelIndex index(
      int row, int column,
      const QModelIndex& parent = QModelIndex()) const override;
  virtual QModelIndex parent(const QModelIndex& index) const override;
  core::DObjectSp IndexToObject(const QModelIndex& index) const;
  QModelIndex ObjectToIndex(const core::DObjectSp& obj) const;

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace qt

}  // namespace dino
