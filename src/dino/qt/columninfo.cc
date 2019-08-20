// Copyright (c) 2018 Akihito Takeuchi
// Distributed under the MIT License : http://opensource.org/licenses/MIT

#include "dino/qt/dobjecttablemodel.h"

#include <QList>
#include <fmt/format.h>

#include "dino/core/dobject.h"

namespace dino {

namespace qt {

struct DValueToQVariant : public boost::static_visitor<QVariant> {
  QVariant operator()(dino::core::DNilType) const {
    return "nil";
  }
  QVariant operator()(int v) const {
    return v;
  }
  QVariant operator()(double v) const {
    return v;
  }
  QVariant operator()(bool v) const {
    return v;
  }
  QVariant operator()(const std::string& v) const {
    return QString::fromStdString(v);
  }
  QVariant operator()(const std::vector<dino::core::DValue>& values) const {
    std::vector<std::string> result_strs;
    std::transform(values.cbegin(), values.cend(),
                   std::back_inserter(results_strs);
                   [this](auto& v) { return boost::apply_visitor(
                       dino::core::detail::DValueToString(',', '(', ')'), v); });
    return QString::fromStdString(
        fmt::format("({})", boost::algorithm::join(result_strs, ",")));
  }
};

ColumnInfo::ColumnInfo(
    const QString& col_name,
    SourceTypeConst source_type,
    const QString& source_name,
    const GetDataFuncType& get_data_func,
    const GetFlagsFuncType& get_flags_func,
    const SetDataFuncType& set_data_func)
    : col_name_(col_name), source_type_(source_type),
      source_name_(source_name),
      get_data_func_(get_data_func),
      get_flags_func_(get_flags_func),
      set_data_func_(set_data_func) {
}

ColumnInfo::~ColumnInfo() = default;

QString ColumnInfo::ColumnName() const {
  return col_name_;
}

SourceTypeConst ColumnInfo::SourceType() const {
  return source_type_;
}

QString ColumnInfo::SourceName() const {
  return source_name_;
}

QVariant ColumnInfo::GetData(const core::DObjectSp& obj, int role) const {
  if (get_data_func_)
    return get_data_func_(obj, role);

  // Default GetData function
  if (role == Qt::DisplayRole) {
    switch (source_type_) {
      case SourceTypeConst::kName:
        return QString::fromStdString(obj->Name());
      case SourceTypeConst::kType:
        return QString::fromStdString(obj->Type());
      case SourceTypeConst::kValue:
        return boost::apply_visitor(
            DValueToQVariant(),
            obj->Get(source_name_.toStdString(), std::string()));
      default:
        break;
    }
  }
  return QVariant();
}

Qt::ItemFlags ColumnInfo::GetFlags(const core::DObjectSp& obj) const {
  if (get_flags_func_)
    return get_flags_func_(obj);
  return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

bool ColumnInfo::SetData(
    const core::DObjectSp& obj, const QVariant& value, int role) const {
  if (set_data_func_)
    return set_data_func_(obj, value, role);
  return false;
}

}  // namespace qt

}  // namespace dino
