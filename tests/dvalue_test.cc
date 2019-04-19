// Copyright (c) 2019 Akihito Takeuchi
// Distributed under the MIT License : http://opensource.org/licenses/MIT

#include "dino/core/dvalue.h"

#include <gtest/gtest.h>

auto& nil = dino::core::nil;
using DValue = dino::core::DValue;
using DValueArray = dino::core::DValueArray;

TEST(DValueTest, IntTest) {
  DValue val(1);
  ASSERT_EQ(val, 1);
  ASSERT_EQ(ToString(val), "1");
}

TEST(DValueTest, DoubleTest) {
  DValue val(1.5);
  ASSERT_EQ(val, 1.5);
  ASSERT_EQ(ToString(val), "1.5");
  DValue val2(2.0);
  ASSERT_EQ(val2, 2.0);
  ASSERT_EQ(ToString(val2), "2.0");
}

TEST(DValueTest, StringTest) {
  DValue val(std::string("test"));
  ASSERT_EQ(val, "test");
  ASSERT_EQ(ToString(val), "\"test\"");
}

TEST(DValueTest, BoolNilTest) {
  DValue val;
  ASSERT_EQ(val, nil);
  ASSERT_EQ(ToString(val), "nil");

  DValue val_true(true);
  ASSERT_EQ(val_true, true);
  ASSERT_EQ(ToString(val_true), "true");

  DValue val_false(false);
  ASSERT_EQ(val_false, false);
  ASSERT_EQ(ToString(val_false), "false");
}

TEST(DValueTest, VectorTest) {
  DValueArray int_values;
  int_values.push_back(1);
  int_values.push_back(5);
  int_values.push_back(10);
  DValue val(int_values);
  ASSERT_EQ(val, int_values);
  ASSERT_TRUE(IsArrayValue(val));
  ASSERT_EQ(ToString(val), "[1,5,10]");
  ASSERT_EQ(ToString(val, ' '),
            "[1 5 10]");
  ASSERT_EQ(ToString(val, ' ', '{', '}'),
            "{1 5 10}");
  ASSERT_EQ(int_values[0], 1);
  ASSERT_EQ(int_values[1], 5);
  ASSERT_EQ(int_values[2], 10);
}
