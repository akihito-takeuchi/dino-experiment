// Copyright (c) 2018 Akihito Takeuchi
// Distributed under the MIT License : http://opensource.org/licenses/MIT

#pragma once

#include <QAbstractTableModel>
#include <functional>
#include <string>
#include <vector>

#include "dino/core/fwd.h"

namespace dino {

namespace qt {

class DinoTableModel : public QAbstractTableModel {
  Q_OBJECT
 public:
  DinoTableModel(const core::DObjectSp& root_obj, QObject* parent = nullptr);
  virtual ~DinoTableModel();
  virtual int rowCount(const QModelIndex& parent = QModelIndex()) const;
  /* virtual int columnCount(const QModelIndex& parent = QModelIndex()) const; */
  virtual QVariant data(
     const QModelIndex& index, int role = Qt::DisplayRole) const;
  virtual Qt::ItemFlags flags(const QModelIndex& index) const;
  virtual bool setData(const QModelIndex& index, const QVariant& value,
                       int role = Qt::EditRole);

 protected:
  virtual QVariant data(
      const core::DObjectSp& row_obj, int column, int role) const = 0;
  virtual Qt::ItemFlags flags(
      const core::DObjectSp& row_obj, int column) const = 0;
  virtual bool setData(const core::DObjectSp& row_obj, int column,
                       const QVariant& value, int role) = 0;

 private:
  core::DObjectSp root_obj_;
};

}  // namespace qt

}  // namespace dino
