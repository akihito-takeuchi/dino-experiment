// Copyright (c) 2018 Akihito Takeuchi
// Distributed under the MIT License : http://opensource.org/licenses/MIT

#include "dino/core/dobject.h"

#include <gtest/gtest.h>
#include <boost/filesystem.hpp>

#include "dino/core/session.h"
#include "dino/core/dexception.h"

namespace dc = dino::core;
namespace fs = boost::filesystem;

auto& nil = dc::nil;

namespace {

const std::string kWspFile = "dino.wsp";
const std::string kObjName1 = "top1";
const std::string kObjName2 = "top2";

}

class DataIOTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    if (fs::exists(kWspFile))
      fs::remove(kWspFile);
    if (fs::exists(kObjName1))
      fs::remove_all(kObjName1);
    if (fs::exists(kObjName2))
      fs::remove_all(kObjName2);
  }
};

TEST_F(DataIOTest, WithoutSave) {
  auto session = dc::Session::Create(kWspFile);
  ASSERT_TRUE(session);
  dc::DObjectSp obj(session->CreateTopLevelObject(kObjName1, kObjName1));
  ASSERT_FALSE(obj->HasKey("test"));
  obj->Put("test", std::string("test value"));
  ASSERT_EQ(obj->Get("test"), "test value");

  ASSERT_FALSE(obj->HasKey("test2"));
  ASSERT_THROW(obj->Get("test2"), dc::DException);
  ASSERT_EQ(obj->Get("test2", nil), nil);
  
  obj->Put("test3", 10);
  ASSERT_TRUE(obj->Get("test3") == 10);
  obj->RemoveKey("test3");
  ASSERT_THROW(obj->RemoveKey("test3"), dc::DException);
  ASSERT_THROW(obj->Get("test3"), dc::DException);
  ASSERT_EQ(obj->Get("test3", 1.25), 1.25);

  obj->Put("test4", false);
  ASSERT_TRUE(obj->Get("test4") == false);
  ASSERT_FALSE(obj->Get("test4") == true);
  session->Save();
}

TEST_F(DataIOTest, SaveReadTest) {
  auto session = dc::Session::Create(kWspFile);
  ASSERT_TRUE(session);
  dc::DObjPath path2(kObjName2);
  auto obj = session->CreateTopLevelObject(kObjName2, kObjName2);
  session->InitTopLevelObjectPath(kObjName2, kObjName2);
  ASSERT_TRUE(obj);
  obj->Put("test", 30);
  ASSERT_TRUE(obj->Get("test") == 30);
  session->PurgeObject(path2);
  ASSERT_THROW(session->PurgeObject(path2), dc::DException);
  
  ASSERT_THROW(session->GetObject(path2), dc::DException);
  obj = session->OpenTopLevelObject(kObjName2, kObjName2);
  obj->SetEditable();
  ASSERT_THROW(obj->Get("test"), dc::DException);
  obj->Put("test", 30);
  ASSERT_TRUE(obj->Get("test") == 30);
  obj->Save();
  session->PurgeObject(path2);

  obj = session->OpenTopLevelObject(kObjName2, kObjName2);
  ASSERT_TRUE(obj->Get("test") == 30);
  session->Save();
}
