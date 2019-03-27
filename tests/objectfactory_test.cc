// Copyright (c) 2019 Akihito Takeuchi
// Distributed under the MIT License : http://opensource.org/licenses/MIT

#include "dino/core/objectfactory.h"

#include <gtest/gtest.h>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string/join.hpp>

#include "dino/core/session.h"
#include "dino/core/dexception.h"

namespace dc = dino::core;
namespace fs = boost::filesystem;

namespace {

const std::string kWspFile = "dino.wsp";
const std::string kTopName1 = "top1";
const std::string kTopName2 = "top2";
const std::string kTopName3 = "top3";

}

class ObjectFactoryTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    if (fs::exists(kWspFile))
      fs::remove(kWspFile);
    for (auto dir_name : {kTopName1, kTopName2, kTopName3})
      if (fs::exists(dir_name))
        fs::remove_all(dir_name);
  }
};

class Top1 : public dc::DObject {
 public:
  using dc::DObject::DObject;
};

class Top2 : public dc::DObject {
 public:
  using dc::DObject::DObject;
};

class Top3 : public dc::DObject {
 public:
  using dc::DObject::DObject;
};

TEST_F(ObjectFactoryTest, DefaultCreateFunc) {
  auto& factory = dc::ObjectFactory::Instance();
  factory.Reset();
  auto session = dc::Session::Create(kWspFile);
  auto top1 = session->CreateObject(dc::DObjPath(kTopName1), kTopName1);
  ASSERT_FALSE(std::dynamic_pointer_cast<Top1>(top1));
  factory.DisableDefault();
  ASSERT_THROW(session->CreateObject(dc::DObjPath(kTopName2), kTopName2),
               dc::DException);
  factory.Register(kTopName1, [](auto& data) { return new Top1(data); });
  auto top1_1 = session->CreateObject(dc::DObjPath(kTopName1 + "_1"), kTopName1);
  ASSERT_TRUE(std::dynamic_pointer_cast<Top1>(top1_1));
  ASSERT_THROW(session->CreateObject(dc::DObjPath(kTopName2 + "_1"), kTopName2),
               dc::DException);
  factory.EnableDefault();
  auto top2_2 = session->CreateObject(dc::DObjPath(kTopName2 + "_2"), kTopName2);
  ASSERT_TRUE(top2_2);
  ASSERT_FALSE(std::dynamic_pointer_cast<Top1>(top2_2));
  ASSERT_FALSE(std::dynamic_pointer_cast<Top2>(top2_2));
}

TEST_F(ObjectFactoryTest, OpenTest) {
  auto& factory = dc::ObjectFactory::Instance();
  factory.Reset();
  auto session = dc::Session::Create(kWspFile);
  {
    auto top1 = session->CreateObject(dc::DObjPath(kTopName1), kTopName1);
    session->InitTopLevelObjectPath(kTopName1, kTopName1);
    top1->Save();
    auto top2 = session->CreateObject(dc::DObjPath(kTopName2), kTopName2);
    session->InitTopLevelObjectPath(kTopName2, kTopName2);
    top2->Save();
  }
  {
    auto top1 = session->OpenTopLevelObject(kTopName1, kTopName1);
    ASSERT_FALSE(std::dynamic_pointer_cast<Top1>(top1));
    auto top2 = session->OpenTopLevelObject(kTopName2, kTopName2);
    ASSERT_FALSE(std::dynamic_pointer_cast<Top2>(top2));
  }
  {
    factory.DisableDefault();
    ASSERT_THROW(session->OpenTopLevelObject(kTopName1, kTopName1),
                 dc::DException);
    ASSERT_THROW(session->OpenTopLevelObject(kTopName2, kTopName2),
                 dc::DException);
  }
  factory.Register(kTopName1, [](auto& data) { return new Top1(data); });
  factory.Register(kTopName2, [](auto& data) { return new Top2(data); });
  {
    auto top1 = session->OpenTopLevelObject(kTopName1, kTopName1);
    ASSERT_TRUE(std::dynamic_pointer_cast<Top1>(top1));
    auto top2 = session->OpenTopLevelObject(kTopName2, kTopName2);
    ASSERT_TRUE(std::dynamic_pointer_cast<Top2>(top2));
  }
}
