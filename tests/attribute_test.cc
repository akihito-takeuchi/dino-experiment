// Copyright (c) 2019 Akihito Takeuchi
// Distributed under the MIT License : http://opensource.org/licenses/MIT

#include "dino/core/dobject.h"

#include <gtest/gtest.h>
#include <boost/filesystem.hpp>
#include "dino/core/session.h"
#include "dino/core/dexception.h"

namespace dc = dino::core;
namespace fs = boost::filesystem;

namespace {

const std::string kWspFile = "dino.wsp";
const std::string kTopName1 = "top1";
const std::string kTopName2 = "top2";
const std::string kChildName1 = "child1";

}  // namespace

class AttributeTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    if (fs::exists(kWspFile))
      fs::remove(kWspFile);
    for (auto dir_name : {kTopName1, kTopName2})
      if (fs::exists(dir_name))
        fs::remove_all(dir_name);
  }
};

TEST_F(AttributeTest, TestLocal) {
  {
    auto session = dc::Session::Create(kWspFile);
    auto top = session->CreateTopLevelObject(kTopName1, kTopName1);
    session->InitTopLevelObjectPath(kTopName1, kTopName1);
    top->SetTemporaryAttr("temp", "temp");
    top->SetAttr("persistent", "persistent");
    ASSERT_EQ(top->Attr("temp"), "temp");
    ASSERT_EQ(top->Attr("persistent"), "persistent");
    ASSERT_TRUE(top->HasAttr("temp"));
    ASSERT_TRUE(top->HasAttr("persistent"));
    ASSERT_TRUE(top->IsTemporaryAttr("temp"));
    ASSERT_FALSE(top->IsTemporaryAttr("persistent"));
    ASSERT_FALSE(top->HasPersistentAttr("temp"));
    ASSERT_TRUE(top->HasPersistentAttr("persistent"));
    ASSERT_THROW(top->Attr("test"), dc::DException);
    ASSERT_THROW(top->RemoveAttr("test"), dc::DException);

    top->SetTemporaryAttr("temp2", "temp2");
    ASSERT_TRUE(top->IsTemporaryAttr("temp2"));
    ASSERT_FALSE(top->HasPersistentAttr("temp2"));
    top->SetAttr("temp2", "temp2");
    ASSERT_FALSE(top->IsTemporaryAttr("temp2"));
    ASSERT_TRUE(top->HasPersistentAttr("temp2"));
    top->Save();
    session->Save();
  }
  {
    auto session = dc::Session::Open(kWspFile);
    auto top = session->OpenObject(kTopName1);
    ASSERT_FALSE(top->HasAttr("temp"));
    ASSERT_TRUE(top->HasAttr("persistent"));
    ASSERT_TRUE(top->HasAttr("temp2"));
    ASSERT_EQ(top->Attr("persistent"), "persistent");
    ASSERT_EQ(top->Attr("temp2"), "temp2");
    top->SetTemporaryAttr("temp3", "temp3");
    ASSERT_EQ(top->Attr("temp3"), "temp3");
    ASSERT_THROW(top->SetAttr("temp4", "temp4"), dc::DException);
    top->RemoveAttr("temp3");
    ASSERT_THROW(top->RemoveAttr("persistent"), dc::DException);
    ASSERT_THROW(top->SetAttr("temp3", "temp3"), dc::DException);

    top->SetEditable();
    ASSERT_FALSE(top->IsDirty());
    top->SetTemporaryAttr("temp3", "temp3");
    top->SetTemporaryAttr("temp4", "temp4");
    top->SetTemporaryAttr("temp5", "temp5");
    ASSERT_TRUE(top->IsTemporaryAttr("temp3"));
    ASSERT_TRUE(top->IsTemporaryAttr("temp4"));
    ASSERT_TRUE(top->IsTemporaryAttr("temp5"));
    ASSERT_FALSE(top->HasPersistentAttr("temp3"));
    ASSERT_FALSE(top->HasPersistentAttr("temp4"));
    ASSERT_FALSE(top->HasPersistentAttr("temp5"));
    ASSERT_FALSE(top->IsDirty());

    top->SetAttr("temp3", "temp3");
    ASSERT_FALSE(top->IsTemporaryAttr("temp3"));
    ASSERT_TRUE(top->IsTemporaryAttr("temp4"));
    ASSERT_TRUE(top->IsTemporaryAttr("temp5"));
    ASSERT_TRUE(top->HasPersistentAttr("temp3"));
    ASSERT_FALSE(top->HasPersistentAttr("temp4"));
    ASSERT_FALSE(top->HasPersistentAttr("temp5"));
    ASSERT_TRUE(top->IsDirty());
    
    top->Save();
    ASSERT_FALSE(top->IsDirty());
    
    top->SetAllAttrsToBeSaved();
    ASSERT_FALSE(top->IsTemporaryAttr("temp3"));
    ASSERT_FALSE(top->IsTemporaryAttr("temp4"));
    ASSERT_FALSE(top->IsTemporaryAttr("temp5"));
    ASSERT_TRUE(top->HasPersistentAttr("temp3"));
    ASSERT_TRUE(top->HasPersistentAttr("temp4"));
    ASSERT_TRUE(top->HasPersistentAttr("temp5"));
    ASSERT_TRUE(top->IsDirty());

    top->Save();
    ASSERT_FALSE(top->IsDirty());

    top->SetTemporaryAttr("temp6", "temp6");
    ASSERT_FALSE(top->IsDirty());
    top->RemoveAttr("temp6");
    ASSERT_FALSE(top->IsDirty());
    top->RemoveAttr("temp5");
    ASSERT_TRUE(top->IsDirty());
  }
}

TEST_F(AttributeTest, TestInherited) {
  {
    auto session = dc::Session::Create(kWspFile);
    auto top = session->CreateTopLevelObject(kTopName1, kTopName1);
    session->InitTopLevelObjectPath(kTopName1, kTopName1);
    top->SetAttr("test1", "test1");
    top->Save();
    auto c =top->CreateChild(kChildName1, kChildName1);
    c->SetAttr("test2", "test2");
    session->Save();
  }
  {
    auto session = dc::Session::Open(kWspFile);
    auto top = session->CreateTopLevelObject(kTopName2, kTopName2);
    top->AddBase(session->OpenObject(kTopName1));
    ASSERT_TRUE(top->IsActual());
    ASSERT_TRUE(top->IsDirty());
    auto c = top->OpenChild(kChildName1, dc::OpenMode::kEditable);
    ASSERT_FALSE(c->IsActual());
    ASSERT_FALSE(c->IsDirty());
    c->SetTemporaryAttr("test3", "test3");
    ASSERT_FALSE(c->IsActual());
    ASSERT_FALSE(c->IsDirty());
    c->SetAttr("test4", "test4");
    ASSERT_TRUE(c->IsActual());
    ASSERT_TRUE(c->IsDirty());
  }
}
