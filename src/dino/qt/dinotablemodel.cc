// Copyright (c) 2018 Akihito Takeuchi
// Distributed under the MIT License : http://opensource.org/licenses/MIT

#include "dino/qt/dinotablemodel.h"

#include "dino/core/dobject.h"

namespace dino {

namespace qt {

DinoTableModel::DinoTableModel(const core::DObjectSp& root_obj, QObject* parent)
    : QAbstractTableModel(parent), root_obj_(root_obj) {
}

DinoTableModel::~DinoTableModel() = default;

int DinoTableModel::rowCount(const QModelIndex&) const {
  return root_obj_->ChildCount();
}

QVariant DinoTableModel::data(const QModelIndex& index, int role) const {
  auto obj = root_obj_->GetChildObject(index.row());
  return data(obj, index.column(), role);
}

Qt::ItemFlags DinoTableModel::flags(const QModelIndex& index) const {
  auto obj = root_obj_->GetChildObject(index.row());
  return flags(obj, index.column());
}

bool DinoTableModel::setData(
    const QModelIndex& index, const QVariant& value, int role) {
  auto obj = root_obj_->GetChildObject(index.row());
  return setData(obj, index.column(), value, role);
}

}  // namespace qt

}  // namespace dino
