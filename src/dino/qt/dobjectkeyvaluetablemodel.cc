// Copyright (c) 2019 Akihito Takeuchi
// Distributed under the MIT License : http://opensource.org/licenses/MIT

#include "dino/qt/dobjectkeyvaluetablemodel.h"

#include "dino/core/dobject.h"

namespace dino {

namespace qt {

namespace {

struct GetDValueTypeStringVisitor : public boost::static_visitor<QString> {
  QString operator()(dino::core::DNilType) const {
    return "nil";
  }
  QString operator()(int) const {
    return "int";
  }
  QString operator()(double) const {
    return "double";
  }
  QString operator()(bool) const {
    return "bool";
  }
  QString operator()(const std::string&) const {
    return "string";
  }
  QString operator()(const std::vector<dino::core::DValue>&) const {
    return "list";
  }
};

QString TypeString(const core::DValue& value) {
  return boost::apply_visitor(GetDValueTypeStringVisitor(), value);
}

struct DValueToStringVisitor : public boost::static_visitor<QString> {
  QString operator()(dino::core::DNilType) const {
    return "nil";
  }
  QString operator()(int v) const {
    return QString::number(v);
  }
  QString operator()(double v) const {
    auto value_str = QString::number(v);
    if (value_str.indexOf('.') < 0
        && value_str.toLower().indexOf('e') < 0)
      value_str += ".0";
    return value_str;
  }
  QString operator()(bool v) const {
    return v ? "true" : "false";
  }
  QString operator()(const std::string& v) const {
    return QString("\"%1\"").arg(QString::fromStdString(v));
  }
  QString operator()(const std::vector<dino::core::DValue>&) const {
    return "[]";
  }
};

QString DefaultValueToString(const core::DValue& value) {
  return boost::apply_visitor(DValueToStringVisitor(), value);
}

core::DValue DefaultStringToValue(const QString& value_str) {
  bool conversion_succeeded;
  auto value_str_trimmed = value_str.trimmed();
  int int_val = value_str_trimmed.toInt(&conversion_succeeded);
  if (conversion_succeeded)
    return int_val;
  double double_val = value_str_trimmed.toDouble(&conversion_succeeded);
  if (conversion_succeeded)
    return double_val;
  if (value_str_trimmed == "nil")
    return dino::core::nil;
  if (value_str_trimmed == "true")
    return true;
  if (value_str_trimmed == "false")
    return false;
  return value_str_trimmed.toStdString();
}

DObjectKeyValueTableModel::CellStyle DefaultGetStyleFunc(const std::string&) {
  return DObjectKeyValueTableModel::CellStyle();
}

}  // namespace

class DObjectKeyValueTableModel::Impl {
 public:
  struct ColInfo {
    ColInfo() = default;
    ColInfo(const QString& name,
            const GetDataFuncType& get_data,
            const GetStyleFuncType& get_style = DefaultGetStyleFunc)
        : name(name), get_data(get_data), get_style(get_style) {
      
    }
    QString name;
    GetDataFuncType get_data;
    GetStyleFuncType get_style;
  };
  Impl(DObjectKeyValueTableModel* self)
      : self(self) {}
  ~Impl() = default;

  int ColumnToID(int col) {
    if (col < col_id_list.count())
      return col_id_list[col];
    return -1;
  }
  int IDToColumn(int id) {
    return col_id_list.indexOf(id);
  }

  void PreUpdate(const dino::core::Command& cmd) {
    std::vector<std::string> keys;
    int row;
    switch (cmd.Type()) {
      case core::CommandType::kValueAdd:
        keys = root_obj->Keys();
        if (std::find(keys.begin(), keys.end(), cmd.Key()) == keys.end()) {
          keys.push_back(cmd.Key());
          std::sort(keys.begin(), keys.end());
          row = std::distance(keys.begin(),
                              std::find(keys.begin(), keys.end(), cmd.Key()));
          self->beginInsertRows(QModelIndex(), row, row);
          row_count_changing = true;
        }
        break;
      case core::CommandType::kValueDelete:
        if (!root_obj->HasNonLocalKey(cmd.Key())) {
          keys = root_obj->Keys();
          row = std::distance(keys.begin(),
                              std::find(keys.begin(), keys.end(), cmd.Key()));
          self->beginRemoveRows(QModelIndex(), row, row);
          row_count_changing = true;
        }
        break;
      case core::CommandType::kAddBaseObject:
      case core::CommandType::kRemoveBaseObject:
        self->beginResetModel();
        break;
      default:
        break;
    }
  }
  void PostUpdate(const dino::core::Command& cmd) {
    bool need_data_changed_signal = false;
    switch (cmd.Type()) {
      case core::CommandType::kValueAdd:
        if (row_count_changing) {
          self->endInsertRows();
          row_count_changing = false;
        } else {
          need_data_changed_signal = true;
        }
        break;
      case core::CommandType::kValueDelete:
        if (row_count_changing) {
          self->endRemoveRows();
          row_count_changing = false;
        } else {
          need_data_changed_signal = true;
        }
        break;
      case core::CommandType::kValueUpdate:
        need_data_changed_signal = true;
        break;
      case core::CommandType::kAddBaseObject:
      case core::CommandType::kRemoveBaseObject:
        self->endResetModel();
        break;
      default:
        break;
    }
    if (need_data_changed_signal) {
      auto keys = root_obj->Keys();
      auto row = std::distance(keys.begin(),
                               std::find(keys.begin(), keys.end(), cmd.Key()));
      emit self->dataChanged(self->createIndex(row, 0),
                             self->createIndex(row, col_id_list.count() - 1));
    }
  }

  DObjectKeyValueTableModel* self;
  core::DObjectSp root_obj;
  core::DObjectSp listen_target;

  DValueToStringFuncType value_to_string_func = DefaultValueToString;
  StringToDValueFuncType string_to_value_func = DefaultStringToValue;

  QMap<int, ColInfo> id_to_col_info;
  QList<int> col_id_list;

  bool row_count_changing = false;
  bool editable = false;
};

DObjectKeyValueTableModel::DObjectKeyValueTableModel(
    const core::DObjectSp& root_obj,
    const core::DObjectSp& listen_to,
    QObject* parent)
    : QAbstractTableModel(parent),
      impl_(std::make_unique<Impl>(this)) {
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

  // Setup defaults
  impl_->id_to_col_info[kKeyColumnID] =
      Impl::ColInfo("Key", GetDataFuncType());
  impl_->id_to_col_info[kValueColumnID] =
      Impl::ColInfo("Value", GetDataFuncType());
  impl_->id_to_col_info[kValueTypeColumnID] =
      Impl::ColInfo("ValueType",
                    [this](auto& key) {
                      return TypeString(this->impl_->root_obj->Get(key));
                    });
  impl_->col_id_list << kKeyColumnID << kValueColumnID;
}

DObjectKeyValueTableModel::~DObjectKeyValueTableModel() = default;

QVariant DObjectKeyValueTableModel::headerData(
    int section, Qt::Orientation orientation, int role) const {
  if (role != Qt::DisplayRole)
    return QVariant();
  if (orientation == Qt::Vertical)
    return QVariant();
  auto col_id = impl_->col_id_list[section];
  return impl_->id_to_col_info[col_id].name;
}

int DObjectKeyValueTableModel::columnCount(const QModelIndex& parent) const {
  Q_UNUSED(parent);
  return impl_->col_id_list.count();
}

int DObjectKeyValueTableModel::rowCount(const QModelIndex& parent) const {
  Q_UNUSED(parent);
  if (!parent.isValid())
    return static_cast<int>(impl_->root_obj->Keys().size());
  else
    return 0;
}

QVariant DObjectKeyValueTableModel::data(
    const QModelIndex& index, int role) const {
  auto key = impl_->root_obj->Keys()[index.row()];
  auto col_id = impl_->ColumnToID(index.column());
  if (role == Qt::DisplayRole) {
    switch (col_id) {
      case ColumnID::kKeyColumnID:
        return QString::fromStdString(key);
      case ColumnID::kValueColumnID:
        return impl_->value_to_string_func(impl_->root_obj->Get(key));
      default:
        return impl_->id_to_col_info[col_id].get_data(key);
    }
  } else if (role == Qt::ForegroundRole) {
    return impl_->id_to_col_info[col_id].get_style(key).foreground_color;
  } else if (role == Qt::BackgroundRole) {
    return impl_->id_to_col_info[col_id].get_style(key).background_color;
  } else if (role == Qt::FontRole) {
    return impl_->id_to_col_info[col_id].get_style(key).font;
  }
  return QVariant();
}

Qt::ItemFlags DObjectKeyValueTableModel::flags(
    const QModelIndex& index) const {
  auto col_type = impl_->ColumnToID(index.column());
  if (col_type == ColumnID::kValueColumnID && impl_->editable)
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;
  return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

bool DObjectKeyValueTableModel::setData(
    const QModelIndex& index, const QVariant& value, int role) {
  if (role != Qt::EditRole || !impl_->editable)
    return false;
  auto col_type = impl_->ColumnToID(index.column());
  if (col_type != ColumnID::kValueColumnID)
    return false;
  auto key = impl_->root_obj->Keys()[index.row()];
  impl_->root_obj->Put(key, impl_->string_to_value_func(value.toString()));
  return true;
}

int DObjectKeyValueTableModel::CreateUserColumn(
    const QString& name, const GetDataFuncType& get_data_func) {
  auto ids = impl_->id_to_col_info.keys();
  auto new_id = ids.last() + 1;
  impl_->id_to_col_info[new_id] = Impl::ColInfo(name, get_data_func);
  return new_id;
}

QList<int> DObjectKeyValueTableModel::DisplayedColumnIDs() const {
  return impl_->col_id_list;
}

void DObjectKeyValueTableModel::InsertColumnIDs(
    int pos, const QList<int>& column_ids) {
  beginInsertColumns(QModelIndex(), pos, pos + column_ids.size() - 1);
  for (auto& col_id : column_ids) {
    impl_->col_id_list.insert(pos, col_id);
    ++ pos;
  }
  endInsertColumns();
}

void DObjectKeyValueTableModel::RemoveColumnIDs(const QList<int>& column_ids) {
  for (auto& col_id : column_ids) {
    auto col = impl_->col_id_list.indexOf(col_id);
    if (col < 0)
      continue;
    beginRemoveColumns(QModelIndex(), col, col);
    impl_->col_id_list.removeOne(col_id);
    endRemoveColumns();
  }
}

void DObjectKeyValueTableModel::SetColumnName(
    int column_id, const QString& name) {
  impl_->id_to_col_info[column_id].name = name;
  auto col = impl_->col_id_list.indexOf(column_id);
  if (col >= 0)
    emit headerDataChanged(Qt::Horizontal, col, col);
}

void DObjectKeyValueTableModel::SetConversionFunc(
    const DValueToStringFuncType& value_to_string_func,
    const StringToDValueFuncType& string_to_value_func) {
  if (value_to_string_func)
    impl_->value_to_string_func = value_to_string_func;
  if (string_to_value_func)
    impl_->string_to_value_func = string_to_value_func;
}

void DObjectKeyValueTableModel::SetStyleFunc(
    int column_id, const GetStyleFuncType& get_style_func) {
  impl_->id_to_col_info[column_id].get_style = get_style_func;
}

bool DObjectKeyValueTableModel::IsEditable() const {
  return impl_->editable;
}

void DObjectKeyValueTableModel::SetEditable(bool editable) {
  impl_->editable = editable;
}

}  // namespace qt

}  // namespace dino
