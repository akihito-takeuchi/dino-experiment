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

QString DValueToString(const core::DValue& value) {
  return boost::apply_visitor(DValueToStringVisitor(), value);
}

}  // namespace

class DObjectKeyValueTableModel::Impl {
 public:
  Impl(DObjectKeyValueTableModel* self)
      : self(self) {}
  ~Impl() = default;

  ColumnType ColumnToType(int col) {
    if (header_labels[col] == key_label)
      return ColumnType::kKey;
    else if (header_labels[col] == value_label)
      return ColumnType::kValue;
    else
      return ColumnType::kType;
  }
  int TypeToColumn(ColumnType type) {
    QString* target;
    switch (type) {
      case ColumnType::kKey:   target = &key_label;   break;
      case ColumnType::kValue: target = &value_label; break;
      case ColumnType::kType:  target = &type_label;  break;
    }
    return header_labels.indexOf(target);
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
      default:
        break;
    }
    if (need_data_changed_signal) {
      auto keys = root_obj->Keys();
      auto row = std::distance(keys.begin(),
                               std::find(keys.begin(), keys.end(), cmd.Key()));
      auto index = self->createIndex(row, TypeToColumn(ColumnType::kValue));
      emit self->dataChanged(index, index);
    }
  }

  DObjectKeyValueTableModel* self;
  core::DObjectSp root_obj;
  core::DObjectSp listen_target;

  DValueToStringFunc value_to_string_func = DValueToString;
  StringToDValueFunc string_to_value_func;
  GetStyleFunc key_col_style_func;
  GetStyleFunc value_col_style_func;
  GetStyleFunc type_col_style_func;

  // Defaults
  QString key_label = "Key";
  QString type_label = "Type";
  QString value_label = "Value";
  QList<QString*> header_labels;
  bool show_value_type = false;
  int type_column = 1;

  bool row_count_changing = false;
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
  impl_->header_labels << &(impl_->key_label) << &(impl_->value_label);
}

DObjectKeyValueTableModel::~DObjectKeyValueTableModel() = default;

bool DObjectKeyValueTableModel::ShowValueType() const {
  return impl_->show_value_type;
}

void DObjectKeyValueTableModel::SetShowValueType(bool show) {
  if (show != impl_->show_value_type) {
    if (show) {
      beginInsertColumns(QModelIndex(), impl_->type_column, impl_->type_column);
      impl_->header_labels.insert(impl_->type_column, &(impl_->type_label));
      impl_->show_value_type = show;
      endInsertColumns();
    } else {
      beginRemoveColumns(QModelIndex(), impl_->type_column, impl_->type_column);
      impl_->header_labels.removeOne(&(impl_->type_label));
      impl_->show_value_type = show;
      endRemoveColumns();
    }
  }
}

QVariant DObjectKeyValueTableModel::headerData(
    int section, Qt::Orientation orientation, int role) const {
  if (role != Qt::DisplayRole)
    return QVariant();
  if (orientation == Qt::Vertical)
    return QVariant();
  return *(impl_->header_labels[section]);
}

int DObjectKeyValueTableModel::columnCount(const QModelIndex& parent) const {
  Q_UNUSED(parent);
  return impl_->header_labels.count();
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
  auto col_type = impl_->ColumnToType(index.column());
  if (role == Qt::DisplayRole) {
    switch (col_type) {
      case ColumnType::kKey:
        return QString::fromStdString(key);
      case ColumnType::kValue:
        return impl_->value_to_string_func(impl_->root_obj->Get(key));
      case ColumnType::kType:
        return TypeString(impl_->root_obj->Get(key));
    }
  } else if (role == Qt::ForegroundRole) {
    switch (col_type) {
      case ColumnType::kKey:
        return impl_->key_col_style_func(key).foreground_color;
      case ColumnType::kValue:
        return impl_->value_col_style_func(key).foreground_color;
      case ColumnType::kType:
        return impl_->type_col_style_func(key).foreground_color;
    }
  } else if (role == Qt::BackgroundRole) {
    switch (col_type) {
      case ColumnType::kKey:
        return impl_->key_col_style_func(key).background_color;
      case ColumnType::kValue:
        return impl_->value_col_style_func(key).background_color;
      case ColumnType::kType:
        return impl_->type_col_style_func(key).background_color;
    }
  } else if (role == Qt::FontRole) {
    switch (col_type) {
      case ColumnType::kKey:
        return impl_->key_col_style_func(key).font;
      case ColumnType::kValue:
        return impl_->value_col_style_func(key).font;
      case ColumnType::kType:
        return impl_->type_col_style_func(key).font;
    }
  }
  return QVariant();
}

Qt::ItemFlags DObjectKeyValueTableModel::flags(
    const QModelIndex& index) const {
  auto col_type = impl_->ColumnToType(index.column());
  if (col_type == ColumnType::kValue)
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;
  return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

bool DObjectKeyValueTableModel::setData(
    const QModelIndex& index, const QVariant& value, int role) {
  if (role != Qt::EditRole)
    return false;
  auto col_type = impl_->ColumnToType(index.column());
  if (col_type != ColumnType::kValue)
    return false;
  auto key = impl_->root_obj->Keys()[index.row()];
  impl_->root_obj->Put(key, impl_->string_to_value_func(value.toString()));
  return true;
}

void DObjectKeyValueTableModel::SetConversionFunc(
    const DValueToStringFunc& value_to_string_func,
    const StringToDValueFunc& string_to_value_func) {
  if (value_to_string_func)
    impl_->value_to_string_func = value_to_string_func;
  if (string_to_value_func)
    impl_->string_to_value_func = string_to_value_func;
}

void DObjectKeyValueTableModel::SetStyleFunc(
    const GetStyleFunc& key_col_style_func,
    const GetStyleFunc& value_col_style_func,
    const GetStyleFunc& type_col_style_func) {
  if (key_col_style_func)
    impl_->key_col_style_func = key_col_style_func;
  if (value_col_style_func)
    impl_->value_col_style_func = value_col_style_func;
  if (type_col_style_func)
    impl_->type_col_style_func = type_col_style_func;
}

}  // namespace qt

}  // namespace dino
