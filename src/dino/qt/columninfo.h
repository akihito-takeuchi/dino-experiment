// Copyright (c) 2019 Akihito Takeuchi
// Distributed under the MIT License : http://opensource.org/licenses/MIT

#pragma once

#include <functional>
#include <QVariant>

#include "dino/core/dvalue.h"
#include "dino/core/dobject.h"

namespace dino {

namespace qt {

QVariant DValueToQVariant(const core::DValue& value);
core::DValue QVariantToDValue(const QVariant& value);

enum class SourceTypeConst {
  kName,
  kType,
  kValue,
  kChildObject
};

using GetDataFuncType =
    std::function<QVariant (const core::DObjectSp& target,
                            int role)>;

using SetDataFuncType =
    std::function<bool (const core::DObjectSp& target,
                        const QVariant& data,
                        int role)>;

using GetFlagsFuncType =
    std::function<Qt::ItemFlags (const core::DObjectSp& target)>;

class ColumnInfo {
 public:
  ColumnInfo(const QString& col_name,
             SourceTypeConst source_type,
             const QString& source_name,
             const GetDataFuncType& get_data_func = GetDataFuncType(),
             const GetFlagsFuncType& get_flags_func = GetFlagsFuncType(),
             const SetDataFuncType& set_data_func = SetDataFuncType());
  ~ColumnInfo();

  QString ColumnName() const;
  SourceTypeConst SourceType() const;
  QString SourceName() const;
  QVariant GetData(const core::DObjectSp& obj,
                    int role = Qt::DisplayRole) const;
  Qt::ItemFlags GetFlags(const core::DObjectSp& obj) const;
  bool SetData(const core::DObjectSp& obj,
               const QVariant& value,
               int role = Qt::EditRole) const;

 private:
  QString col_name_;
  SourceTypeConst source_type_;
  QString source_name_;
  GetDataFuncType get_data_func_;
  GetFlagsFuncType get_flags_func_;
  SetDataFuncType set_data_func_;
};

}  // namespace qt

}  // namespace dino
