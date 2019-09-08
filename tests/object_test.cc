// Copyright (c) 2018 Akihito Takeuchi
// Distributed under the MIT License : http://opensource.org/licenses/MIT

#include "dino/core/dobject.h"

#include <gtest/gtest.h>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string/join.hpp>

#include "dino/core/session.h"
#include "dino/core/dexception.h"

namespace dc = dino::core;
namespace fs = boost::filesystem;
auto& nil = dc::nil;

namespace {

const std::string kWspFile = "dino.wsp";
const std::string kTopName1 = "top1";
const std::string kTopName2 = "top2";
const std::string kTopName3 = "top3";
const std::string kTopName4 = "top4";
const std::string kTopName5 = "top5";
const std::string kTopName6 = "top6";
const std::string kTopName7 = "top7";
const std::string kChildName1 = "child1";
const std::string kChildName2 = "child2";
const std::string kChildName3 = "child3";
const std::string kChildName4 = "child4";

}

class ObjectTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    if (fs::exists(kWspFile))
      fs::remove(kWspFile);
    for (auto dir_name
             : {kTopName1, kTopName2, kTopName3, kTopName4,
               kTopName5, kTopName6, kTopName7,
                kChildName1, kChildName2, kChildName3, kChildName4})
      if (fs::exists(dir_name))
        fs::remove_all(dir_name);
  }
};

TEST_F(ObjectTest, ExpectFail) {
  auto session = dc::Session::Create(kWspFile);
  dc::DObjPath top_path2(kTopName2);
  std::string child_path_str1 = kTopName2 + "/" + kChildName1;
  dc::DObjPath child_path1(child_path_str1);
  ASSERT_THROW(session->CreateObject(child_path1, "test"), dc::DException);
  ASSERT_THROW(session->GetObject(top_path2), dc::DException);
  auto top = session->CreateTopLevelObject(kTopName1, kTopName1);
  ASSERT_THROW(session->CreateTopLevelObject(kTopName1, kTopName1),
               dc::DException);
  ASSERT_TRUE(top);
  ASSERT_THROW(session->GetObject(child_path1), dc::DException);
  auto child = top->CreateChild(kChildName1, "test");
  ASSERT_TRUE(child);
  ASSERT_THROW(top->Save(), dc::DException);
}

TEST_F(ObjectTest, WriteRead) {
  auto session = dc::Session::Create(kWspFile);
  dc::DObjPath top_path1(kTopName1);
  {
    auto top = session->CreateTopLevelObject(kTopName1, kTopName1);
    ASSERT_TRUE(top);
    ASSERT_THROW(top->Get("TEST"), dc::DException);
    ASSERT_TRUE(top->Get("TEST", 100) == 100);
    ASSERT_FALSE(top->HasKey("TEST"));
    top->Put("TEST", 100);
    ASSERT_TRUE(top->Get("TEST") == 100);
    session->PurgeObject(top_path1);
  }
  {
    dc::DObjectSp top;
    ASSERT_THROW(top = session->GetObject(top_path1),
                 dc::DException);
    top = session->CreateTopLevelObject(kTopName1, kTopName1);
    ASSERT_TRUE(top);
    ASSERT_THROW(top->Get("TEST"), dc::DException);
    
    top->Put("TEST", 100);
    ASSERT_TRUE(top->Get("TEST") == 100);
    session->InitTopLevelObjectPath(kTopName1, kTopName1);
    top->Save();
    session->PurgeObject(top_path1);
  }
  {
    auto top = session->OpenTopLevelObject(kTopName1, kTopName1);
    ASSERT_TRUE(top);
    ASSERT_TRUE(top->Get("TEST") == 100);
  }
  session->Save();
}

TEST_F(ObjectTest, Hierarchy) {
  dc::DObjPath top_obj_path(kTopName2);
  auto session = dc::Session::Create(kWspFile);
  auto top = session->CreateTopLevelObject(kTopName2, kTopName2);
  ASSERT_TRUE(top);
  ASSERT_THROW(session->GetObject(dc::DObjPath(kTopName2 + "/" + kChildName1)),
               dc::DException);
  auto child1 = top->CreateChild(kChildName1, kChildName1);
  ASSERT_TRUE(child1);
  auto child2 = child1->CreateChild(kChildName2, kChildName2);
  child2->Put("test", "value1");
  session->InitTopLevelObjectPath(kTopName2, kTopName2);
  child2->Save();

  session->PurgeObject(top_obj_path);

  auto child1_path = top_obj_path.ChildPath(kChildName1);
  auto child2_path = child1_path.ChildPath(kChildName2);
  ASSERT_THROW(session->GetObject(top_obj_path), dc::DException);
  ASSERT_THROW(session->GetObject(child1_path), dc::DException);
  ASSERT_THROW(session->GetObject(child2_path), dc::DException);
  ASSERT_THROW(session->OpenObject(child1_path), dc::DException);
  ASSERT_THROW(session->OpenObject(child2_path), dc::DException);

  top = session->OpenTopLevelObject(kTopName2, kTopName2);
  ASSERT_TRUE(top);
  ASSERT_EQ(top->Type(), kTopName2);
  auto child_info_list = top->Children();
  ASSERT_EQ(child_info_list.size(), 1u);
  ASSERT_EQ(child_info_list[0].Name(), kChildName1);
  ASSERT_TRUE(top->HasChild(kChildName1));
  ASSERT_FALSE(top->HasChild(kChildName2));

  ASSERT_THROW(session->GetObject(child1_path), dc::DException);
  child1 = session->OpenObject(child1_path);
  ASSERT_TRUE(child1);
  child_info_list = child1->Children();
  ASSERT_EQ(child_info_list.size(), 1u);
  ASSERT_EQ(child_info_list[0].Name(), kChildName2);
  ASSERT_TRUE(child1->HasChild(kChildName2));
  ASSERT_FALSE(child1->HasChild(kChildName1));

  ASSERT_THROW(session->GetObject(child2_path), dc::DException);
  child2 = session->OpenObject(child2_path);
  ASSERT_TRUE(child2);
  child_info_list = child2->Children();
  ASSERT_EQ(child_info_list.size(), 0u);
  ASSERT_FALSE(child2->HasChild(kChildName2));
  ASSERT_FALSE(child2->HasChild(kChildName1));
}

TEST_F(ObjectTest, Flat) {
  auto child1_type = kChildName1 + "_data";
  auto child2_type = kChildName2 + "_data";
  auto child3_type = kChildName3 + "_data";
  auto child4_type = kChildName4 + "_data";
  auto path1_str = boost::join(
      std::vector<std::string>{kTopName3, kChildName1}, "/");
  auto path2_str = boost::join(
      std::vector<std::string>{kTopName3, kChildName1, kChildName2}, "/");
  auto path3_str = boost::join(
      std::vector<std::string>{kTopName3, kChildName1, kChildName3}, "/");
  auto path4_str = boost::join(
        std::vector<std::string>{kTopName3, kChildName4}, "/");
  dc::DObjPath path2(path2_str);
  dc::DObjPath path3(path3_str);
  dc::DObjPath path4(path4_str);
  {
    auto session = dc::Session::Create(kWspFile);
    auto top = session->CreateTopLevelObject(kTopName3, kTopName3);
    session->InitTopLevelObjectPath(kTopName3, kTopName3);
    ASSERT_FALSE(top->IsDirty());
    ASSERT_TRUE(fs::exists(kTopName3));
 
    auto child1 = top->CreateChild(kChildName1, child1_type);
    ASSERT_FALSE(top->IsDirty());
    ASSERT_FALSE(child1->IsDirty());
    ASSERT_TRUE(fs::exists(kTopName3));
    ASSERT_TRUE(fs::exists(path1_str));
 
    auto child2 = child1->CreateChild(kChildName2, child2_type);
    ASSERT_FALSE(top->IsDirty());
    ASSERT_FALSE(child1->IsDirty());
    ASSERT_FALSE(child2->IsDirty());
    ASSERT_TRUE(fs::exists(kTopName3));
    ASSERT_TRUE(fs::exists(path1_str));
    ASSERT_TRUE(fs::exists(path2_str));
 
    top->SetChildFlat(kChildName1);
    ASSERT_TRUE(top->IsDirty());
    ASSERT_TRUE(child1->IsDirty());
    ASSERT_FALSE(child2->IsDirty());
    ASSERT_TRUE(fs::exists(kTopName3));
    ASSERT_FALSE(fs::exists(path1_str));
    ASSERT_FALSE(fs::exists(path2_str));

    auto child3 = child1->CreateChild(kChildName3, child3_type);
    auto child4 = top->CreateChild(kChildName4, child4_type);
    ASSERT_TRUE(fs::exists(kTopName3));
    ASSERT_FALSE(fs::exists(path1_str));
    ASSERT_FALSE(fs::exists(path2_str));
    ASSERT_FALSE(fs::exists(path3_str));
    ASSERT_TRUE(fs::exists(path4_str));

    ASSERT_TRUE(top->IsDirty());
    ASSERT_TRUE(child1->IsDirty());
    ASSERT_FALSE(child2->IsDirty());
    ASSERT_FALSE(child3->IsDirty());
    ASSERT_FALSE(child4->IsDirty());
    
    top->Put("ValueTop1", 100);
    top->Put("ValueTop2", std::string("test"));
    top->Put("ValueTop3", 100.5);
    child1->Put("Child1Value1", 100);
    child1->Put("Child1Value2", 3.14);
    child1->Put("Child1Value3", std::string("test"));
    child2->Put("Child2Value1", std::string("This is test value"));
    child2->Put("Child2Value2", -300);
    child3->Put("Child3Value1", true);
    child3->Put("Child3Value2", nil);
    child4->Put("Child4Value1", false);
    child4->Put("Child4Value2", 1234);

    ASSERT_TRUE(top->IsDirty());
    ASSERT_TRUE(child1->IsDirty());
    ASSERT_TRUE(child2->IsDirty());
    ASSERT_TRUE(child3->IsDirty());
    ASSERT_TRUE(child4->IsDirty());

    ASSERT_FALSE(top->IsFlattened());
    ASSERT_TRUE(child1->IsFlattened());
    ASSERT_TRUE(top->IsChildFlat(kChildName1));
    ASSERT_TRUE(child2->IsFlattened());
    ASSERT_TRUE(child3->IsFlattened());
    ASSERT_FALSE(child4->IsFlattened());
    
    top->Save();
    ASSERT_THROW(child1->Save(), dc::DException);
    ASSERT_THROW(child2->Save(), dc::DException);
    ASSERT_THROW(child3->Save(), dc::DException);
    child4->Save();

    ASSERT_FALSE(top->IsDirty());
    ASSERT_FALSE(child1->IsDirty());
    ASSERT_FALSE(child2->IsDirty());
    ASSERT_FALSE(child3->IsDirty());
    ASSERT_FALSE(child4->IsDirty());

    ASSERT_TRUE(fs::exists(kTopName3));
    ASSERT_FALSE(fs::exists(path1_str));
    ASSERT_FALSE(fs::exists(path2_str));
    ASSERT_FALSE(fs::exists(path3_str));
    ASSERT_TRUE(fs::exists(path4_str));

    session->Save();
  }
  {
    ASSERT_THROW(dc::Session::Create(kWspFile),
                 dc::DException);
    auto session = dc::Session::Open(kWspFile);
    ASSERT_TRUE(session);
    auto names = session->TopObjectNames();
    ASSERT_EQ(names.size(), 1u);
    ASSERT_THROW(session->CreateTopLevelObject(kTopName3, kTopName3),
                 dc::DException);

    auto top = session->OpenObject(dc::DObjPath(kTopName3));
    ASSERT_TRUE(top);
    ASSERT_FALSE(top->IsDirty());
    auto children_of_top = top->Children();
    ASSERT_EQ(children_of_top.size(), 2u);
    ASSERT_EQ(children_of_top[0].Name(), kChildName1);
    ASSERT_EQ(children_of_top[1].Name(), kChildName4);
    ASSERT_EQ(top->Type(), kTopName3);

    auto child1 = session->GetObject(dc::DObjPath(path1_str));
    ASSERT_TRUE(child1);
    ASSERT_TRUE(child1->IsFlattened());
    ASSERT_FALSE(child1->IsDirty());
    auto children_of_child1 = child1->Children();
    ASSERT_EQ(children_of_child1.size(), 2u);
    ASSERT_EQ(children_of_child1[0].Name(), kChildName2);
    ASSERT_EQ(children_of_child1[1].Name(), kChildName3);
    ASSERT_EQ(child1->Type(), child1_type);

    auto child2 = session->GetObject(path2);
    child2 = session->CreateObject(path2, child2_type);
    ASSERT_TRUE(child2);
    ASSERT_FALSE(child2->IsDirty());
    ASSERT_EQ(child2->Children().size(), 0u);
    ASSERT_EQ(child2->Type(), child2_type);

    auto child3 = session->GetObject(path3);
    ASSERT_TRUE(child3);
    ASSERT_FALSE(child3->IsDirty());
    ASSERT_EQ(child3->Children().size(), 0u);
    ASSERT_EQ(child3->Type(), child3_type);

    ASSERT_THROW(session->GetObject(path4),
                 dc::DException);
    auto child4 = session->OpenObject(path4);
    ASSERT_TRUE(child4);
    ASSERT_FALSE(child4->IsDirty());
    ASSERT_EQ(child4->Children().size(), 0u);
    ASSERT_EQ(child4->Type(), child4_type);

    ASSERT_TRUE(top->Get("ValueTop1") == 100);
    ASSERT_TRUE(top->Get("ValueTop2") == "test");
    ASSERT_TRUE(top->Get("ValueTop3") == 100.5);
    ASSERT_TRUE(child1->Get("Child1Value1") == 100);
    ASSERT_TRUE(child1->Get("Child1Value2") == 3.14);
    ASSERT_TRUE(child1->Get("Child1Value3") == "test");
    ASSERT_TRUE(child2->Get("Child2Value1") == "This is test value");
    ASSERT_TRUE(child2->Get("Child2Value2") == -300);
    ASSERT_TRUE(child3->Get("Child3Value1") == true);
    ASSERT_TRUE(child3->Get("Child3Value2") == nil);
    ASSERT_TRUE(child4->Get("Child4Value1") == false);
    ASSERT_TRUE(child4->Get("Child4Value2") == 1234);

    top->RefreshChildren();
    children_of_top = top->Children();
    ASSERT_EQ(children_of_top.size(), 2u);
    ASSERT_EQ(children_of_top[0].Name(), kChildName1);
    ASSERT_EQ(children_of_top[1].Name(), kChildName4);

    ASSERT_TRUE(fs::exists(kTopName3));
    ASSERT_FALSE(fs::exists(path1_str));
    ASSERT_FALSE(fs::exists(path2_str));
    ASSERT_FALSE(fs::exists(path3_str));
    ASSERT_TRUE(fs::exists(path4_str));
  }
}

TEST_F(ObjectTest, FlattenBeforeDirInit) {
  auto child1_type = kChildName1 + "_data";
  auto child2_type = kChildName2 + "_data";
  auto path1_str = boost::join(
      std::vector<std::string>{kTopName4, kChildName1}, "/");
  auto path2_str = boost::join(
      std::vector<std::string>{kTopName4, kChildName1, kChildName2}, "/");
  dc::DObjPath path1(path1_str);
  dc::DObjPath path2(path2_str);
  {
    auto session = dc::Session::Create();
    auto top = session->CreateTopLevelObject(kTopName4, kTopName4);
    ASSERT_FALSE(top->IsDirty());
    ASSERT_FALSE(fs::exists(kTopName4));

    auto child1 = top->CreateChild(kChildName1, child1_type);
    ASSERT_FALSE(top->IsDirty());
    ASSERT_FALSE(child1->IsDirty());
    ASSERT_FALSE(fs::exists(kTopName4));
    ASSERT_FALSE(fs::exists(path1_str));

    auto child2 = child1->CreateChild(kChildName2, child2_type);
    ASSERT_FALSE(top->IsDirty());
    ASSERT_FALSE(child1->IsDirty());
    ASSERT_FALSE(child2->IsDirty());
    ASSERT_FALSE(fs::exists(kTopName4));
    ASSERT_FALSE(fs::exists(path1_str));
    ASSERT_FALSE(fs::exists(path2_str));

    top->SetChildFlat(kChildName1);
    ASSERT_TRUE(top->IsDirty());
    ASSERT_TRUE(child1->IsDirty());
    ASSERT_FALSE(child2->IsDirty());
    ASSERT_FALSE(fs::exists(kTopName4));
    ASSERT_FALSE(fs::exists(path1_str));
    ASSERT_FALSE(fs::exists(path2_str));

    session->InitTopLevelObjectPath(kTopName4, kTopName4);
    
    top->Put("ValueTop1", 100);
    top->Put("ValueTop2", std::string("test"));
    top->Put("ValueTop3", 100.5);
    child1->Put("Child1Value1", 100);
    child1->Put("Child1Value2", 3.14);
    child1->Put("Child1Value3", std::string("test"));
    child2->Put("Child2Value1", std::string("This is test value"));
    child2->Put("Child2Value2", -300);

    ASSERT_TRUE(top->IsDirty());
    ASSERT_TRUE(child1->IsDirty());
    ASSERT_TRUE(child2->IsDirty());

    ASSERT_FALSE(top->IsFlattened());
    ASSERT_TRUE(child1->IsFlattened());
    ASSERT_TRUE(top->IsChildFlat(kChildName1));
    ASSERT_TRUE(child2->IsFlattened());
    
    top->Save();
    ASSERT_THROW(child1->Save(), dc::DException);
    ASSERT_THROW(child2->Save(), dc::DException);

    ASSERT_FALSE(top->IsDirty());
    ASSERT_FALSE(child1->IsDirty());
    ASSERT_FALSE(child2->IsDirty());

    ASSERT_TRUE(fs::exists(kTopName4));
    ASSERT_FALSE(fs::exists(path1_str));
    ASSERT_FALSE(fs::exists(path2_str));
  }
  {
    auto session = dc::Session::Create();
    auto top = session->OpenTopLevelObject(kTopName4, kTopName4);
    ASSERT_EQ(top->Children().size(), 1u);
    ASSERT_FALSE(top->IsFlattened());
    ASSERT_FALSE(top->IsDirty());

    auto child1 = session->GetObject(path1);
    ASSERT_EQ(child1->Children().size(), 1u);
    ASSERT_TRUE(child1->IsFlattened());
    ASSERT_FALSE(child1->IsDirty());

    auto child2 = session->GetObject(path2);
    ASSERT_EQ(child2->Children().size(), 0u);
    ASSERT_TRUE(child2->IsFlattened());
    ASSERT_FALSE(child2->IsDirty());

    ASSERT_TRUE(top->Get("ValueTop1") == 100);
    ASSERT_TRUE(top->Get("ValueTop2") == "test");
    ASSERT_TRUE(top->Get("ValueTop3") == 100.5);
    ASSERT_TRUE(child1->Get("Child1Value1") == 100);
    ASSERT_TRUE(child1->Get("Child1Value2") == 3.14);
    ASSERT_TRUE(child1->Get("Child1Value3") == "test");
    ASSERT_TRUE(child2->Get("Child2Value1") == "This is test value");
    ASSERT_TRUE(child2->Get("Child2Value2") == -300);

    ASSERT_TRUE(fs::exists(kTopName4));
    ASSERT_FALSE(fs::exists(path1_str));
    ASSERT_FALSE(fs::exists(path2_str));
  }
}

TEST_F(ObjectTest, LockFile) {
  auto session = dc::Session::Create();
  {
    auto top = session->CreateTopLevelObject(kTopName5, "top");
    session->InitTopLevelObjectPath(kTopName5, kTopName5);
    ASSERT_TRUE(fs::is_directory(kTopName5));
    ASSERT_TRUE(fs::exists(kTopName5 + "/top.json"));
    ASSERT_TRUE(fs::exists(kTopName5 + "/top.json.lock"));
    top->SetReadOnly();
    ASSERT_FALSE(fs::exists(kTopName5 + "/top.json.lock"));
    top->SetEditable();
    ASSERT_TRUE(fs::exists(kTopName5 + "/top.json.lock"));
  }
  ASSERT_TRUE(fs::exists(kTopName5 + "/top.json"));
  ASSERT_FALSE(fs::exists(kTopName5 + "/top.json.lock"));
}

TEST_F(ObjectTest, ExtraFileInObjectDir) {
  auto session = dc::Session::Create();
  auto top = session->CreateTopLevelObject(kTopName6, "top");
  session->InitTopLevelObjectPath(kTopName6, kTopName6);
  auto child = top->CreateChild(kChildName1, "child");
  auto child_str = kTopName6 + "/" + kChildName1;
  ASSERT_TRUE(fs::is_directory(kTopName6));
  ASSERT_TRUE(fs::exists(kTopName6 + "/top.json"));
  ASSERT_TRUE(fs::exists(kTopName6 + "/top.json.lock"));
  ASSERT_TRUE(fs::is_directory(child_str));
  ASSERT_TRUE(fs::exists(child_str + "/child.json"));
  ASSERT_TRUE(fs::exists(child_str + "/child.json.lock"));
  top->SetChildFlat(kChildName1);
  ASSERT_FALSE(fs::is_directory(child_str));
  ASSERT_FALSE(fs::exists(child_str + "/child.json"));
  ASSERT_FALSE(fs::exists(child_str + "/child.json.lock"));
  
  auto child2 = top->CreateChild(kChildName2, "child");
  auto child2_str = kTopName6 + "/" + kChildName2;
  ASSERT_TRUE(fs::is_directory(child2_str));
  ASSERT_TRUE(fs::exists(child2_str + "/child.json"));
  ASSERT_TRUE(fs::exists(child2_str + "/child.json.lock"));
  {
    std::ofstream f((child2_str + "/test.txt").c_str());
  }
  ASSERT_TRUE(fs::exists(child2_str + "/test.txt"));
  top->SetChildFlat(kChildName2);
  ASSERT_TRUE(fs::is_directory(child2_str));
  ASSERT_FALSE(fs::exists(child2_str + "/child.json"));
  ASSERT_FALSE(fs::exists(child2_str + "/child.json.lock"));
  ASSERT_TRUE(fs::exists(child2_str + "/test.txt"));
}

TEST_F(ObjectTest, StoreArray) {
  {
    auto session = dc::Session::Create(kWspFile);
    auto top = session->CreateTopLevelObject(kTopName7, "top");
    session->InitTopLevelObjectPath(kTopName7, kTopName7);
    dc::DValueArray values;
    values.push_back(1);
    values.push_back(1.5);
    values.push_back(false);
    values.push_back(std::string("test"));
    values.push_back(nil);
    auto child = top->CreateChild(kChildName1, "child");
    child->Put("test_key", values);
    child->Save();
    session->Save();
  }
  {
    auto session = dc::Session::Open(kWspFile);
    session->OpenObject(kTopName7);
    auto child = session->OpenObject(kTopName7 + "/" + kChildName1);
    auto values = boost::get<dc::DValueArray>(child->Get("test_key"));
    ASSERT_EQ(values[0], 1);
    ASSERT_EQ(values[1], 1.5);
    ASSERT_EQ(values[2], false);
    ASSERT_EQ(values[3], std::string("test"));
    ASSERT_EQ(values[4], nil);
  }
}

