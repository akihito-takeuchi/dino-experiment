// Copyright (c) 2019 Akihito Takeuchi
// Distributed under the MIT License : http://opensource.org/licenses/MIT

#include "dino/qt/dobjecttreemodel.h"

#include "dino/core/command.h"
#include "dino/core/session.h"
#include "dino/core/dobject.h"

namespace dino {

namespace qt {

namespace {

core::DObjectSp GetObjectAt(const core::DObjectSp& obj, int row) {
  auto child_info = obj->Children()[row];
  return obj->GetChildObject(child_info.Name());
}

int GetRow(const core::DObjectSp& obj, const core::DObjectSp& child) {
  auto child_info_list = obj->Children();
  auto target_name = child->Name();
  return std::distance(
      child_info_list.cbegin(),
      std::find_if(child_info_list.cbegin(), child_info_list.cend(),
                   [&target_name](auto& child_info) {
                     return target_name == child_info.Name(); }));
}

}  // namespace

class DObjectTreeModel::Impl {
 public:
  Impl(DObjectTreeModel* self) : self(self) {}
  ~Impl() = default;
  void PreUpdate(const core::Command& cmd) {
    switch (cmd.Type()) {
      case core::CommandType::kAddBaseObject:
      case core::CommandType::kRemoveBaseObject:
        self->beginResetModel();
        break;
      case core::CommandType::kAddChild:
      case core::CommandType::kAddFlattenedChild: {
        auto session = root_obj->GetSession();
        auto obj = session->GetObject(cmd.ObjPath());
        auto child_name = cmd.TargetObjectName();
        auto children = cmd.PrevChildren();
        auto index = self->ObjectToIndex(obj);
        bool already_exists = false;
        int row = 0;
        for (auto& child_info : cmd.PrevChildren()) {
          if (child_info.Name() == child_name) {
            already_exists = true;
            break;
          } else if (child_name < child_info.Name()) {
            break;
          }
          row ++;
        }
        if (!already_exists)
          self->beginInsertRows(index, row, row);
        break;
      }
      case core::CommandType::kDeleteChild: {
        auto session = root_obj->GetSession();
        auto obj = session->GetObject(cmd.ObjPath());
        auto child_name = cmd.TargetObjectName();
        auto children = cmd.PrevChildren();
        auto index = self->ObjectToIndex(obj);
        auto row = std::distance(
            children.cbegin(),
            std::find_if(
                children.cbegin(), children.cend(),
                [&child_name](auto& obj_info) {
                  return obj_info.Name() == child_name; }));
        self->beginRemoveRows(index, row, row);
        break;
      }
      default:
        break;
    }
  }
  void PostUpdate(const core::Command& cmd) {
    switch (cmd.Type()) {
      case core::CommandType::kValueAdd:
      case core::CommandType::kValueUpdate:
      case core::CommandType::kValueDelete: {
        auto session = root_obj->GetSession();
        auto obj = session->GetObject(cmd.ObjPath());
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
  QModelIndex GetIndexFromRoute(
      std::vector<core::DObjectSp>& route, const QModelIndex& current) {
    if (route.empty())
      return current;
    auto target = route.back();
    route.pop_back();
    for (int row = 0; row < self->rowCount(current); ++ row) {
      auto child_index = self->index(row, 0, current);
      if (child_index.internalId() == target->ObjectId())
        return GetIndexFromRoute(route, child_index);
    }
    return QModelIndex();
  }
  core::DObjectSp root_obj;
  QList<ColumnInfo> col_info_list;
  DObjectTreeModel* self;
};

DObjectTreeModel::DObjectTreeModel(const core::DObjectSp& root_obj,
                                   QObject* parent)
    : QAbstractItemModel(parent), impl_(std::make_unique<Impl>(this)) {
  impl_->root_obj = root_obj;
  impl_->root_obj->AddListener([this](auto& cmd) {
      this->impl_->PreUpdate(cmd);
    }, core::ListenerCallPoint::kPre);
  impl_->root_obj->AddListener([this](auto& cmd) {
      this->impl_->PostUpdate(cmd);
    }, core::ListenerCallPoint::kPost);
}

DObjectTreeModel::~DObjectTreeModel() = default;

void DObjectTreeModel::InsertColumn(
    int col, const QList<ColumnInfo>& col_info_list) {
  beginInsertColumns(QModelIndex(), col, col + col_info_list.size() - 1);
  for (auto& col_info : col_info_list) {
    impl_->col_info_list.insert(col, col_info);
    col ++;
  }
  endInsertColumns();
}

void DObjectTreeModel::RemoveColumns(int first, int last) {
  beginRemoveColumns(QModelIndex(), first, last);
  while (last >= first) {
    impl_->col_info_list.removeAt(first);
    last --;
  }
  endRemoveColumns();
}

QVariant DObjectTreeModel::headerData(
    int section, Qt::Orientation orient, int role) const {
  if (role != Qt::DisplayRole)
    return QVariant();
  if (orient == Qt::Vertical)
    return QVariant();
  return impl_->col_info_list[section].ColumnName();
}

int DObjectTreeModel::columnCount(const QModelIndex& /*parent*/) const {
  return impl_->col_info_list.count();
}

int DObjectTreeModel::rowCount(const QModelIndex& parent) const {
  return static_cast<int>(IndexToObject(parent)->ChildCount());
}

QVariant DObjectTreeModel::data(const QModelIndex& index, int role) const {
  auto obj = IndexToObject(index);
  return impl_->col_info_list[index.column()].GetData(obj, role);
}

Qt::ItemFlags DObjectTreeModel::flags(const QModelIndex& index) const {
  auto obj = IndexToObject(index);
  return impl_->col_info_list[index.column()].GetFlags(obj);
}

bool DObjectTreeModel::setData(
    const QModelIndex& index, const QVariant& data, int role) {
  auto obj = IndexToObject(index);
  return impl_->col_info_list[index.column()].SetData(obj, data, role);
}

QModelIndex DObjectTreeModel::index(int row, int column,
                                    const QModelIndex& parent) const {
  auto parent_obj = IndexToObject(parent);
  auto obj = GetObjectAt(parent_obj, row);
  return createIndex(row, column, obj->ObjectId());
}

QModelIndex DObjectTreeModel::parent(const QModelIndex& index) const {
  if (!index.isValid())
    return QModelIndex();

  auto obj = IndexToObject(index);
  if (obj->ObjectId() == impl_->root_obj->ObjectId())
    return QModelIndex();

  auto parent_obj = obj->Parent();
  if (!parent_obj)
    return QModelIndex();
  if (parent_obj->ObjectId() == impl_->root_obj->ObjectId())
    return QModelIndex();

  auto grand_parent_obj = parent_obj->Parent();
  if (!grand_parent_obj)
    return QModelIndex();
  auto row = GetRow(grand_parent_obj, parent_obj);
  return createIndex(row, 0, parent_obj->ObjectId());
}

core::DObjectSp DObjectTreeModel::IndexToObject(const QModelIndex& index) const {
  if (!index.isValid())
    return impl_->root_obj;
  return impl_->root_obj->GetSession()->GetObject(index.internalId());
}

QModelIndex DObjectTreeModel::ObjectToIndex(const core::DObjectSp& obj) const {
  std::vector<core::DObjectSp> route_to_root;
  core::DObjectSp current_obj = obj;
  while (current_obj && current_obj->ObjectId() != impl_->root_obj->ObjectId()) {
    route_to_root.push_back(current_obj);
    current_obj = current_obj->Parent();
  }
  return impl_->GetIndexFromRoute(route_to_root, QModelIndex());
}

}  // namespace qt

}  // namespace dino
