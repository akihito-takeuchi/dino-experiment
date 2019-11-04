// Copyright (c) 2018 Akihito Takeuchi
// Distributed under the MIT License : http://opensource.org/licenses/MIT

#include "dino/qt/utils.h"

namespace dino {

namespace qt {

namespace {

struct DToQConverter : public boost::static_visitor<QVariant> {
  QVariant operator()(dino::core::DNilType) const {
    return QVariant();
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
    QVariantList result;
    std::transform(values.cbegin(), values.cend(),
                   std::back_inserter(result),
                   [](auto& v) {
                     return boost::apply_visitor(DToQConverter(), v); });
    return result;
  }
};

}  // namespace

QVariant DValueToQVariant(const core::DValue& value) {
  return boost::apply_visitor(DToQConverter(), value);
}

core::DValue QVariantToDValue(const QVariant& value) {
  core::DValue result;
  if (!value.isValid()) {
    return dino::core::nil;
  } else if (value.type() == QVariant::Int) {
    result = value.toInt();
  } else if (value.type() == QVariant::Double) {
    result = value.toDouble();
  } else if (value.type() == QVariant::Bool) {
    result = value.toBool();
  } else if (value.type() == QVariant::String) {
    result = value.toString().toStdString();
  } else if (value.type() == QVariant::List) {
    core::DValueArray array;
    for (auto& elem : value.toList())
      array.push_back(QVariantToDValue(elem));
    result = array;
  }
  return result;
}

}  // namespace qt

}  // namespace dino
