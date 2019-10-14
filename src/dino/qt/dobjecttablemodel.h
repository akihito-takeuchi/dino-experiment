// Copyright (c) 2019 Akihito Takeuchi
// Distributed under the MIT License : http://opensource.org/licenses/MIT

#pragma once

#include <QAbstractTableModel>
#include <QString>
#include <QStringList>
#include <functional>
#include <memory>

#include "dino/core/fwd.h"
#include "dino/qt/columninfo.h"

namespace dino {

namespace qt {

class DObjectTableModel : public QAbstractTableModel {
  Q_OBJECT
 public:
  DObjectTableModel(const core::DObjectSp& root_obj,
                    const core::DObjectSp& listen_to = nullptr,
                    QObject* parent = nullptr);
  virtual ~DObjectTableModel();
  virtual void InsertColumns(int col, const QList<ColumnInfo>& col_info_list);
  virtual void RemoveColumns(int first, int last);
  QModelIndex ObjectToIndex(const core::DObjectSp& obj) const;
  core::DObjectSp IndexToObject(const QModelIndex& index) const;
  virtual int rowCount(const QModelIndex& parent = QModelIndex()) const;
  virtual int columnCount(const QModelIndex& parent = QModelIndex()) const;
  virtual QVariant headerData(
      int section, Qt::Orientation orient, int role) const;
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
