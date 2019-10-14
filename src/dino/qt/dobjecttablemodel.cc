// Copyright (c) 2018 Akihito Takeuchi
// Distributed under the MIT License : http://opensource.org/licenses/MIT

#include "dino/qt/dobjecttablemodel.h"

#include <QList>

#include "dino/core/session.h"
#include "dino/core/dobject.h"

namespace dino {

namespace qt {

class DObjectTableModel::Impl {
 public:
  Impl(DObjectTableModel* self) : self(self) {}
  ~Impl() = default;
  void PreUpdate(const dino::core::Command& cmd) {
    auto session = root_obj->GetSession();
    auto obj = session->OpenObject(cmd.ObjPath());
    switch (cmd.Type()) {
      case core::CommandType::kAddBaseObject:
      case core::CommandType::kRemoveBaseObject:
        self->beginResetModel();
        break;
      case core::CommandType::kAddChild:
      case core::CommandType::kAddFlattenedChild: {
        auto child_name = cmd.TargetObjectName();
        auto children = obj->Children();
        auto index = self->ObjectToIndex(obj);
        auto row = std::distance(
            children.cbegin(),
            std::find_if(
                children.cbegin(), children.cend(),
                [child_name](auto& obj_info) {
                  return obj_info.Name() < child_name;}));
        self->beginInsertRows(index, row, row);
        break;
      }
      case core::CommandType::kDeleteChild: {
        auto child_name = cmd.TargetObjectName();
        auto children = cmd.PrevChildren();
        auto index = self->ObjectToIndex(obj);
        auto row = std::distance(
            children.cbegin(),
            std::find_if(
                children.cbegin(), children.cend(),
                [child_name](auto& obj) { return obj.Name() == child_name; }));
        self->beginRemoveRows(index, row, row);
        break;
      }
      default:
        break;
    }
  }
  void PostUpdate(const dino::core::Command& cmd) {
    auto session = root_obj->GetSession();
    auto obj = session->OpenObject(cmd.ObjPath());
    switch (cmd.Type()) {
      case core::CommandType::kValueAdd:
      case core::CommandType::kValueUpdate:
      case core::CommandType::kValueDelete: {
        auto col = KeyToCol(cmd.Key());
        auto index = self->ObjectToIndex(obj);
        if (col < self->columnCount(index))
          emit self->dataChanged(index.sibling(index.row(), col),
                                 index.sibling(index.row(), col));
        break;
      }
      case core::CommandType::kAddBaseObject:
      case core::CommandType::kRemoveBaseObject:
        self->endResetModel();
        break;
      case core::CommandType::kAddChild:
      case core::CommandType::kAddFlattenedChild:
        self->endInsertRows();
        break;
      case core::CommandType::kDeleteChild:
        self->endRemoveRows();
        break;
      default:
        break;
    }
  }
  int KeyToCol(const std::string& key_) {
    auto key = QString::fromStdString(key_);
    int col = 0;
    for (auto& col_info : col_info_list) {
      if (col_info.SourceName() == key)
        return col;
      col ++;
    }
    return col;
  }
  core::DObjectSp root_obj;
  core::DObjectSp listen_target;
  QList<ColumnInfo> col_info_list;
  DObjectTableModel* self;
};

DObjectTableModel::DObjectTableModel(const core::DObjectSp& root_obj,
                                     const core::DObjectSp& listen_to,
                                     QObject* parent)
    : QAbstractTableModel(parent), impl_(std::make_unique<Impl>(this)) {
  impl_->root_obj = root_obj;
  impl_->listen_target = listen_to;
  if (!impl_->listen_target)
    impl_->listen_target = root_obj;
  impl_->listen_target->AddListener([this](auto& cmd) {
      this->impl_->PreUpdate(cmd);
    }, dino::core::ListenerCallPoint::kPre);
  impl_->listen_target->AddListener([this](auto& cmd) {
      this->impl_->PostUpdate(cmd);
    }, dino::core::ListenerCallPoint::kPost);
}

DObjectTableModel::~DObjectTableModel() = default;

void DObjectTableModel::InsertColumns(
    int col, const QList<ColumnInfo>& col_info_list) {
  beginInsertColumns(QModelIndex(), col, col + col_info_list.size() - 1);
  for (auto& col_info : col_info_list) {
    impl_->col_info_list.insert(col, col_info);
    ++ col;
  }
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

QModelIndex DObjectTableModel::ObjectToIndex(const core::DObjectSp& obj) const {
  auto child = obj;
  auto root_id = impl_->root_obj->ObjectId();
  while (child->Parent()
         && root_id != child->Parent()->ObjectId())
    child = child->Parent();
  if (!child->Parent())
    return QModelIndex();
  auto children = impl_->root_obj->Children();
  auto target_name = obj->Name();
  auto row = std::distance(
      children.cbegin(),
      std::find_if(children.cbegin(), children.cend(),
                   [&target_name](auto& obj_info) {
                     return obj_info.Name() == target_name;} ));
  return index(row, 0, QModelIndex());
}

core::DObjectSp DObjectTableModel::IndexToObject(const QModelIndex& index) const {
  if (!index.isValid())
    return impl_->root_obj;
  auto children = impl_->root_obj->Children();
  if (index.row() >= static_cast<int>(children.size()))
    return nullptr;
  return impl_->root_obj->OpenChild(children[index.row()].Name());
}

int DObjectTableModel::rowCount(const QModelIndex&) const {
  return impl_->root_obj->ChildCount();
}

int DObjectTableModel::columnCount(const QModelIndex&) const {
  return impl_->col_info_list.size();
}

QVariant DObjectTableModel::headerData(
    int section, Qt::Orientation orient, int role) const {
  if (role != Qt::DisplayRole)
    return QAbstractTableModel::headerData(section, orient, role);;
  if (orient == Qt::Vertical)
    return QAbstractTableModel::headerData(section, orient, role);;
  return impl_->col_info_list[section].ColumnName();
}

QVariant DObjectTableModel::data(const QModelIndex& index, int role) const {
  auto obj = impl_->root_obj->GetChildAt(index.row());
  auto col_info = impl_->col_info_list[index.column()];
  return col_info.GetData(obj, role);
}

Qt::ItemFlags DObjectTableModel::flags(const QModelIndex& index) const {
  auto obj = impl_->root_obj->GetChildAt(index.row());
  auto col_info = impl_->col_info_list[index.column()];
  return col_info.GetFlags(obj);
}

bool DObjectTableModel::setData(
    const QModelIndex& index, const QVariant& value, int role) {
  auto obj = impl_->root_obj->GetChildAt(index.row());
  obj->SetEditable();
  auto col_info = impl_->col_info_list[index.column()];
  return col_info.SetData(obj, value, role);
}

}  // namespace qt

}  // namespace dino
