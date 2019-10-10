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
const std::string kWspFile2 = "dino2.wsp";
const std::string kWspFile3 = "dino3.wsp";
const std::string kTopName1 = "top1";
const std::string kTopName2 = "top2";
const std::string kTopName3 = "top3";
const std::string kTopName4 = "top4";
const std::string kParentName1 = "parent1";
const std::string kParentName2 = "parent2";
const std::string kChildName1 = "child1";
const std::string kChildName2 = "child2";
const std::string kChildName3 = "child3";
const std::string kChildName4 = "child4";
const std::string kChildName5 = "child5";

}

namespace dino {

namespace core {

std::ostream& operator<<(std::ostream& os, const DObjInfo& info) {
  os << "DObjInfo(" << info.Name() << "," << info.Type() << ","
     << info.Path().String() << ","
     << (info.IsValid() ? "valid" : "invalid") <<  ","
     << (info.IsActual() ? "local" : "remote") << ")\n";
  return os;
}

}  // namespace core

}  // namespace dino

using StringVector = std::vector<std::string>;

class InheritTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    if (fs::exists(kWspFile))
      fs::remove(kWspFile);
    if (fs::exists(kWspFile2))
      fs::remove(kWspFile2);
    if (fs::exists(kWspFile3))
      fs::remove(kWspFile3);
    for (auto dir_name
             : {kTopName1, kTopName2, kTopName3, kTopName4})
      if (fs::exists(dir_name))
        fs::remove_all(dir_name);
  }
};

TEST_F(InheritTest, SimpleIneritance) {
  auto session = dc::Session::Create();
  dc::DObjPath path1(boost::join(
      StringVector{kTopName1, kChildName1}, "/"));
  dc::DObjPath path2(boost::join(
      StringVector{kTopName1, kChildName2}, "/"));
  auto top1 = session->CreateTopLevelObject(kTopName1, "test");
  auto child1 = top1->CreateChild(kChildName1, "test");
  auto child2 = top1->CreateChild(kChildName2, "test");

  child2->Put("test", 100);
  ASSERT_FALSE(child1->HasKey("test"));
  ASSERT_EQ(child2->Get("test"), 100);
  ASSERT_EQ(child1->Bases().size(), 0u);
  ASSERT_EQ(child2->Bases().size(), 0u);

  child1->AddBase(child2);
  ASSERT_TRUE(child1->HasKey("test"));
  ASSERT_EQ(child1->Get("test"), 100);
  ASSERT_EQ(child2->Get("test"), 100);
  ASSERT_EQ(child1->WhereIsKey("test"), path2);
  ASSERT_FALSE(child1->IsLocalKey("test"));
  ASSERT_EQ(child1->Bases().size(), 1u);
  ASSERT_EQ(child1->Bases()[0]->Path(), child2->Path());
  ASSERT_NE(child1->Bases()[0]->Path(), child1->Path());
  ASSERT_EQ(child2->Bases().size(), 0u);

  child1->Put("test", 200);
  ASSERT_TRUE(child1->HasKey("test"));
  ASSERT_EQ(child1->Get("test"), 200);
  ASSERT_EQ(child2->Get("test"), 100);
  ASSERT_EQ(child1->WhereIsKey("test"), path1);
  ASSERT_TRUE(child1->IsLocalKey("test"));

  child1->RemoveBase(child2);
  ASSERT_TRUE(child1->HasKey("test"));
  ASSERT_EQ(child1->Get("test"), 200);
  ASSERT_EQ(child2->Get("test"), 100);
  ASSERT_EQ(child1->WhereIsKey("test"), path1);
  ASSERT_TRUE(child1->IsLocalKey("test"));
  ASSERT_EQ(child1->Bases().size(), 0u);
  ASSERT_EQ(child2->Bases().size(), 0u);

  child1->RemoveKey("test");
  ASSERT_THROW(child1->Get("test"), dc::DException);
  ASSERT_THROW(child1->WhereIsKey("test"), dc::DException); 
}

TEST_F(InheritTest, LoadSaveInerited) {
  dc::DObjPath path1(boost::join(
      StringVector{kTopName2, kChildName1}, "/"));
  dc::DObjPath path2(boost::join(
      StringVector{kTopName2, kChildName2}, "/"));
  {
    auto session = dc::Session::Create(kWspFile);
    auto top = session->CreateTopLevelObject(kTopName2, "test");
    session->InitTopLevelObjectPath(kTopName2, kTopName2);
    auto child1 = top->CreateChild(kChildName1, "test");
    auto child2 = top->CreateChild(kChildName2, "test");
    child1->AddBase(child2);
    child1->Put("test1", 100);
    child2->Put("test1", 200);
    child2->Put("test2", 300);
    ASSERT_TRUE(child1->HasKey("test1"));
    ASSERT_TRUE(child1->HasKey("test2"));
    ASSERT_TRUE(child1->IsLocalKey("test1"));
    ASSERT_FALSE(child1->IsLocalKey("test2"));
    ASSERT_TRUE(child2->HasKey("test1"));
    ASSERT_TRUE(child2->HasKey("test2"));
    ASSERT_TRUE(child1->IsLocalKey("test1"));
    ASSERT_TRUE(child2->IsLocalKey("test2"));
    ASSERT_EQ(child1->Get("test1"), 100);
    ASSERT_EQ(child1->Get("test2"), 300);
    ASSERT_EQ(child2->Get("test1"), 200);
    ASSERT_EQ(child2->Get("test2"), 300);
    child1->Save();
    child2->Save();
    session->Save();
  }
  {
    auto session = dc::Session::Open(kWspFile);
    auto top = session->OpenObject(kTopName2);
    auto child1 = session->OpenObject(path1);
    auto child2 = session->OpenObject(path2);
    ASSERT_EQ(child1->Get("test1"), 100);
    ASSERT_EQ(child1->Get("test2"), 300);
    ASSERT_EQ(child2->Get("test1"), 200);
    ASSERT_EQ(child2->Get("test2"), 300);
  }
}

TEST_F(InheritTest, PurgeBase) {
  dc::DObjPath path1(boost::join(
      StringVector{kTopName3, kChildName1}, "/"));
  dc::DObjPath path2(boost::join(
      StringVector{kTopName3, kChildName2}, "/"));
  {
    auto session = dc::Session::Create(kWspFile);
    auto top = session->CreateTopLevelObject(kTopName3, "test");
    session->InitTopLevelObjectPath(kTopName3, kTopName3);
    auto child1 = top->CreateChild(kChildName1, "test");
    auto child2 = top->CreateChild(kChildName2, "test");
    child1->AddBase(child2);
    child1->Put("test1", 100);
    child2->Put("test1", 200);
    child2->Put("test2", 300);

    ASSERT_TRUE(child1->HasKey("test1"));
    ASSERT_TRUE(child1->HasKey("test2"));
    ASSERT_TRUE(child1->IsLocalKey("test1"));
    ASSERT_FALSE(child1->IsLocalKey("test2"));
    ASSERT_TRUE(child2->HasKey("test1"));
    ASSERT_TRUE(child2->HasKey("test2"));
    ASSERT_TRUE(child1->IsLocalKey("test1"));
    ASSERT_TRUE(child2->IsLocalKey("test2"));
    ASSERT_EQ(child1->Get("test1"), 100);
    ASSERT_EQ(child1->Get("test2"), 300);
    ASSERT_EQ(child2->Get("test1"), 200);
    ASSERT_EQ(child2->Get("test2"), 300);
    child2->Save();

    session->PurgeObject(child2->Path());
    ASSERT_TRUE(child1->HasKey("test1"));
    ASSERT_TRUE(child1->HasKey("test2"));
    ASSERT_TRUE(child1->IsLocalKey("test1"));
    ASSERT_FALSE(child1->IsLocalKey("test2"));
    ASSERT_TRUE(child1->IsLocalKey("test1"));
    ASSERT_EQ(child1->Get("test1"), 100);
    ASSERT_EQ(child1->Get("test2"), 300);
  }
}

TEST_F(InheritTest, InheritedChildren) {
  dc::DObjPath path1(boost::join(
      StringVector{kTopName4, kParentName1}, "/"));
  dc::DObjPath path2(boost::join(
      StringVector{kTopName4, kParentName2}, "/"));
  
  dc::DObjPath path1_1(boost::join(
      StringVector{kTopName4, kParentName1, kChildName1}, "/"));
  dc::DObjPath path1_2(boost::join(
      StringVector{kTopName4, kParentName1, kChildName2}, "/"));
  dc::DObjPath path2_2(boost::join(
      StringVector{kTopName4, kParentName2, kChildName2}, "/"));
  dc::DObjPath path2_3(boost::join(
      StringVector{kTopName4, kParentName2, kChildName3}, "/"));

  auto session = dc::Session::Create(kWspFile);
  auto top = session->CreateTopLevelObject(kTopName4, "test");
  auto parent1 = top->CreateChild(kParentName1, "parent");
  auto parent2 = top->CreateChild(kParentName2, "parent");

  parent1->CreateChild(kChildName1, "child");
  parent2->CreateChild(kChildName2, "child");
  ASSERT_EQ(parent1->Children().size(), 1u);
  ASSERT_EQ(parent2->Children().size(), 1u);

  parent1->AddBase(parent2);
  
  ASSERT_EQ(parent1->Children().size(), 2u);
  ASSERT_EQ(parent2->Children().size(), 1u);
  
  ASSERT_TRUE(parent1->HasChild(kChildName1));
  ASSERT_TRUE(parent1->HasChild(kChildName2));
  ASSERT_TRUE(parent1->IsActualChild(kChildName1));
  ASSERT_FALSE(parent1->IsActualChild(kChildName2));
 
  ASSERT_FALSE(parent2->HasChild(kChildName1));
  ASSERT_TRUE(parent2->HasChild(kChildName2));
  ASSERT_THROW(parent2->IsActualChild(kChildName1), dc::DException);
  ASSERT_TRUE(parent2->IsActualChild(kChildName2));

  ASSERT_THROW(parent1->IsActualChild(kChildName3), dc::DException);
  
  parent1->CreateChild(kChildName2, "child");
  ASSERT_EQ(parent1->Children().size(), 2u);
  ASSERT_EQ(parent2->Children().size(), 1u);

  ASSERT_TRUE(parent1->HasChild(kChildName1));
  ASSERT_TRUE(parent1->HasChild(kChildName2));
  ASSERT_TRUE(parent1->IsActualChild(kChildName1));
  ASSERT_TRUE(parent1->IsActualChild(kChildName2));
  
  ASSERT_FALSE(parent2->HasChild(kChildName1));
  ASSERT_TRUE(parent2->HasChild(kChildName2));
  ASSERT_THROW(parent2->IsActualChild(kChildName1), dc::DException);
  ASSERT_TRUE(parent2->IsActualChild(kChildName2));

  ASSERT_THROW(parent1->IsActualChild(kChildName3), dc::DException);
  
  parent2->CreateChild(kChildName3, "child");
  ASSERT_EQ(parent1->Children().size(), 3u);
  ASSERT_EQ(parent2->Children().size(), 2u);

  ASSERT_TRUE(parent1->HasChild(kChildName1));
  ASSERT_TRUE(parent1->HasChild(kChildName2));
  ASSERT_TRUE(parent1->HasChild(kChildName3));
  ASSERT_TRUE(parent1->IsActualChild(kChildName1));
  ASSERT_TRUE(parent1->IsActualChild(kChildName2));
  ASSERT_FALSE(parent1->IsActualChild(kChildName3));
  
  ASSERT_FALSE(parent2->HasChild(kChildName1));
  ASSERT_TRUE(parent2->HasChild(kChildName2));
  ASSERT_TRUE(parent2->HasChild(kChildName3));
  ASSERT_THROW(parent2->IsActualChild(kChildName1), dc::DException);
  ASSERT_TRUE(parent2->IsActualChild(kChildName2));
  ASSERT_TRUE(parent2->IsActualChild(kChildName3));

  ASSERT_EQ(parent1->Children()[0].Name(), kChildName1);
  ASSERT_EQ(parent1->Children()[1].Name(), kChildName2);
  ASSERT_EQ(parent1->Children()[2].Name(), kChildName3);

  ASSERT_EQ(parent2->Children()[0].Name(), kChildName2);
  ASSERT_EQ(parent2->Children()[1].Name(), kChildName3);

  parent1->RefreshChildren();
  
  ASSERT_EQ(parent1->Children()[0].Name(), kChildName1);
  ASSERT_EQ(parent1->Children()[1].Name(), kChildName2);
  ASSERT_EQ(parent1->Children()[2].Name(), kChildName3);

  ASSERT_TRUE(parent1->HasChild(kChildName1));
  ASSERT_TRUE(parent1->HasChild(kChildName2));
  ASSERT_TRUE(parent1->HasChild(kChildName3));
  ASSERT_TRUE(parent1->IsActualChild(kChildName1));
  ASSERT_TRUE(parent1->IsActualChild(kChildName2));
  ASSERT_FALSE(parent1->IsActualChild(kChildName3));
}

// Command stack version

class InheritTestWithCommandStack : public ::testing::Test {
 protected:
  virtual void SetUp() {
    if (fs::exists(kWspFile))
      fs::remove(kWspFile);
    for (auto dir_name
             : {kTopName1, kTopName2, kTopName3, kTopName4})
      if (fs::exists(dir_name))
        fs::remove_all(dir_name);
  }
};

TEST_F(InheritTestWithCommandStack, SimpleIneritance) {
  auto session = dc::Session::Create();
  dc::DObjPath path1(boost::join(
      StringVector{kTopName1, kChildName1}, "/"));
  dc::DObjPath path2(boost::join(
      StringVector{kTopName1, kChildName2}, "/"));
  auto top1 = session->CreateTopLevelObject(kTopName1, "test");
  top1->EnableCommandStack();
  auto child1 = top1->CreateChild(kChildName1, "test");
  auto child2 = top1->CreateChild(kChildName2, "test");

  child2->Put("test", 100);
  ASSERT_FALSE(child1->HasKey("test"));
  ASSERT_EQ(child2->Get("test"), 100);
  ASSERT_EQ(child1->Bases().size(), 0u);
  ASSERT_EQ(child2->Bases().size(), 0u);

  child1->AddBase(child2);
  ASSERT_TRUE(child1->HasKey("test"));
  ASSERT_EQ(child1->Get("test"), 100);
  ASSERT_EQ(child2->Get("test"), 100);
  ASSERT_EQ(child1->WhereIsKey("test"), path2);
  ASSERT_FALSE(child1->IsLocalKey("test"));
  ASSERT_EQ(child1->Bases().size(), 1u);
  ASSERT_EQ(child1->Bases()[0]->Path(), child2->Path());
  ASSERT_NE(child1->Bases()[0]->Path(), child1->Path());
  ASSERT_EQ(child2->Bases().size(), 0u);

  child1->Put("test", 200);
  ASSERT_TRUE(child1->HasKey("test"));
  ASSERT_EQ(child1->Get("test"), 200);
  ASSERT_EQ(child2->Get("test"), 100);
  ASSERT_EQ(child1->WhereIsKey("test"), path1);
  ASSERT_TRUE(child1->IsLocalKey("test"));

  child1->RemoveBase(child2);
  ASSERT_TRUE(child1->HasKey("test"));
  ASSERT_EQ(child1->Get("test"), 200);
  ASSERT_EQ(child2->Get("test"), 100);
  ASSERT_EQ(child1->WhereIsKey("test"), path1);
  ASSERT_TRUE(child1->IsLocalKey("test"));
  ASSERT_EQ(child1->Bases().size(), 0u);
  ASSERT_EQ(child2->Bases().size(), 0u);

  child1->RemoveKey("test");
  ASSERT_THROW(child1->Get("test"), dc::DException);
  ASSERT_THROW(child1->WhereIsKey("test"), dc::DException);
}

TEST_F(InheritTestWithCommandStack, InheritedChildren) {
  dc::DObjPath path1(boost::join(
      StringVector{kTopName4, kParentName1}, "/"));
  dc::DObjPath path2(boost::join(
      StringVector{kTopName4, kParentName2}, "/"));
  
  dc::DObjPath path1_1(boost::join(
      StringVector{kTopName4, kParentName1, kChildName1}, "/"));
  dc::DObjPath path1_2(boost::join(
      StringVector{kTopName4, kParentName1, kChildName2}, "/"));
  dc::DObjPath path2_2(boost::join(
      StringVector{kTopName4, kParentName2, kChildName2}, "/"));
  dc::DObjPath path2_3(boost::join(
      StringVector{kTopName4, kParentName2, kChildName3}, "/"));

  auto session = dc::Session::Create(kWspFile);
  auto top = session->CreateTopLevelObject(kTopName4, "test");
  top->EnableCommandStack();
  auto parent1 = top->CreateChild(kParentName1, "parent");
  auto parent2 = top->CreateChild(kParentName2, "parent");

  parent1->CreateChild(kChildName1, "child");
  parent2->CreateChild(kChildName2, "child");
  ASSERT_EQ(parent1->Children().size(), 1u);
  ASSERT_EQ(parent2->Children().size(), 1u);

  parent1->AddBase(parent2);
  
  ASSERT_EQ(parent1->Children().size(), 2u);
  ASSERT_EQ(parent2->Children().size(), 1u);
  
  ASSERT_TRUE(parent1->HasChild(kChildName1));
  ASSERT_TRUE(parent1->HasChild(kChildName2));
  ASSERT_TRUE(parent1->IsActualChild(kChildName1));
  ASSERT_FALSE(parent1->IsActualChild(kChildName2));
 
  ASSERT_FALSE(parent2->HasChild(kChildName1));
  ASSERT_TRUE(parent2->HasChild(kChildName2));
  ASSERT_THROW(parent2->IsActualChild(kChildName1), dc::DException);
  ASSERT_TRUE(parent2->IsActualChild(kChildName2));

  ASSERT_THROW(parent1->IsActualChild(kChildName3), dc::DException);
  
  parent1->CreateChild(kChildName2, "child");
  ASSERT_EQ(parent1->Children().size(), 2u);
  ASSERT_EQ(parent2->Children().size(), 1u);

  ASSERT_TRUE(parent1->HasChild(kChildName1));
  ASSERT_TRUE(parent1->HasChild(kChildName2));
  ASSERT_TRUE(parent1->IsActualChild(kChildName1));
  ASSERT_TRUE(parent1->IsActualChild(kChildName2));
  
  ASSERT_FALSE(parent2->HasChild(kChildName1));
  ASSERT_TRUE(parent2->HasChild(kChildName2));
  ASSERT_THROW(parent2->IsActualChild(kChildName1), dc::DException);
  ASSERT_TRUE(parent2->IsActualChild(kChildName2));

  ASSERT_THROW(parent1->IsActualChild(kChildName3), dc::DException);
  
  parent2->CreateChild(kChildName3, "child");
  ASSERT_EQ(parent1->Children().size(), 3u);
  ASSERT_EQ(parent2->Children().size(), 2u);

  ASSERT_TRUE(parent1->HasChild(kChildName1));
  ASSERT_TRUE(parent1->HasChild(kChildName2));
  ASSERT_TRUE(parent1->HasChild(kChildName3));
  ASSERT_TRUE(parent1->IsActualChild(kChildName1));
  ASSERT_TRUE(parent1->IsActualChild(kChildName2));
  ASSERT_FALSE(parent1->IsActualChild(kChildName3));
  
  ASSERT_FALSE(parent2->HasChild(kChildName1));
  ASSERT_TRUE(parent2->HasChild(kChildName2));
  ASSERT_TRUE(parent2->HasChild(kChildName3));
  ASSERT_THROW(parent2->IsActualChild(kChildName1), dc::DException);
  ASSERT_TRUE(parent2->IsActualChild(kChildName2));
  ASSERT_TRUE(parent2->IsActualChild(kChildName3));

  ASSERT_EQ(parent1->Children()[0].Name(), kChildName1);
  ASSERT_EQ(parent1->Children()[1].Name(), kChildName2);
  ASSERT_EQ(parent1->Children()[2].Name(), kChildName3);

  ASSERT_EQ(parent2->Children()[0].Name(), kChildName2);
  ASSERT_EQ(parent2->Children()[1].Name(), kChildName3);

  parent1->RefreshChildren();
  
  ASSERT_EQ(parent1->Children()[0].Name(), kChildName1);
  ASSERT_EQ(parent1->Children()[1].Name(), kChildName2);
  ASSERT_EQ(parent1->Children()[2].Name(), kChildName3);

  ASSERT_TRUE(parent1->HasChild(kChildName1));
  ASSERT_TRUE(parent1->HasChild(kChildName2));
  ASSERT_TRUE(parent1->HasChild(kChildName3));
  ASSERT_TRUE(parent1->IsActualChild(kChildName1));
  ASSERT_TRUE(parent1->IsActualChild(kChildName2));
  ASSERT_FALSE(parent1->IsActualChild(kChildName3));
}

TEST_F(InheritTest, DeepInheritance) {
  // base object tree
  // top1             : { "key1" : "value1", "key2" : 2, "key3" : nil }
  //   +- child1      : { "key1" : "child1" }
  //   |    +- child3 : { "key2" : 10.0 }
  //   +- child2      : { "key3" : true }

  // Create base tree
  {
    auto session = dc::Session::Create(kWspFile2);
    auto top = session->CreateTopLevelObject(kTopName1, kTopName1);
    session->InitTopLevelObjectPath(kTopName1, kTopName1);
    top->Put("key1", std::string("value1"));
    top->Put("key2", 2);
    top->Put("key3", nil);
    auto c1 = top->CreateChild(kChildName1, kChildName1);
    c1->Put("key1", std::string("child1"));
    auto c2 = top->CreateChild(kChildName2, kChildName2);
    c2->Put("key3", true);
    auto c3 = c1->CreateChild(kChildName3, kChildName3);
    c3->Put("key2", 10.0);
    
    top->Save();
    c1->Save();
    c2->Save();
    c3->Save();
    session->Save();
  }
  // Test base tree
  {
    auto session = dc::Session::Open(kWspFile2);
    auto top = session->OpenObject(kTopName1);
    ASSERT_EQ(top->Get("key1"), std::string("value1"));
    ASSERT_EQ(top->Get("key2"), 2);
    ASSERT_EQ(top->Get("key3"), nil);
    ASSERT_EQ(top->ChildCount(), 2u);
    ASSERT_EQ(top->Children()[0].Name(), kChildName1);
    ASSERT_EQ(top->Children()[1].Name(), kChildName2);
    auto c1 = top->OpenChild(kChildName1);
    ASSERT_EQ(c1->Get("key1"), std::string("child1"));
    auto c2 = top->OpenChild(kChildName2);
    ASSERT_EQ(c2->Get("key3"), true);
    auto c3 = c1->OpenChild(kChildName3);
    ASSERT_EQ(c3->Get("key2"), 10.0);
  }
  // create another tree which inherits base
  {
    auto session = dc::Session::Open(kWspFile2);
    auto top = session->CreateTopLevelObject(kTopName2,kTopName2);
    session->InitTopLevelObjectPath(kTopName2, kTopName2);
    top->AddBase(session->OpenObject(kTopName1));
    ASSERT_EQ(top->ChildCount(), 2u);
    auto children = top->Children();
    ASSERT_EQ(children[0].Name(), kChildName1);
    ASSERT_EQ(children[1].Name(), kChildName2);
    auto c1 = top->OpenChild(kChildName1);
    ASSERT_FALSE(c1->IsActual());
    ASSERT_FALSE(c1->IsDirty());
    ASSERT_EQ(c1->Name(), std::string(kChildName1));
    ASSERT_EQ(c1->Path(), dc::DObjPath(fmt::format("{}/{}", kTopName2, kChildName1)));
    ASSERT_EQ(c1->Get("key1"), std::string("child1"));
    ASSERT_EQ(c1->ChildCount(), 1u);
    auto c2 = top->OpenChild(kChildName2);
    ASSERT_EQ(c2->ChildCount(), 0u);
    ASSERT_EQ(c2->Get("key3"), true);
    auto c3 = c1->OpenChild(kChildName3);
    ASSERT_EQ(c3->ChildCount(), 0u);
    ASSERT_EQ(c3->Get("key2"), 10.0);
    ASSERT_FALSE(c3->IsDirty());
    top->Save(true);
    session->Save();
  }
  // Check inherited, and put data&save
  {
    auto session = dc::Session::Open(kWspFile2);
    auto top = session->OpenObject(kTopName2);
    ASSERT_EQ(top->EffectiveBases().size(), 1u);
    auto children = top->Children();
    ASSERT_EQ(top->ChildCount(), 2u);
    ASSERT_EQ(children[0].Name(), kChildName1);
    ASSERT_EQ(children[1].Name(), kChildName2);
    ASSERT_FALSE(children[0].IsActual());
    ASSERT_FALSE(children[1].IsActual());
    auto c1 = top->OpenChild(kChildName1);
    ASSERT_FALSE(c1->IsActual());
    ASSERT_EQ(top->ChildInfo(kChildName1), children[0]);
    ASSERT_FALSE(top->ChildInfo(kChildName1).IsActual());
    ASSERT_EQ(c1->Name(), std::string(kChildName1));
    ASSERT_EQ(c1->Path(), dc::DObjPath(fmt::format("{}/{}", kTopName2, kChildName1)));
    ASSERT_EQ(c1->Get("key1"), std::string("child1"));
    ASSERT_EQ(c1->ChildCount(), 1u);
    ASSERT_EQ(c1->Bases().size(), 0u);
    ASSERT_EQ(c1->EffectiveBases().size(), 1u);
    auto c2 = top->OpenChild(kChildName2);
    ASSERT_EQ(c2->ChildCount(), 0u);
    ASSERT_EQ(c2->Get("key3"), true);
    auto c3 = c1->OpenChild(kChildName3);
    ASSERT_EQ(c3->ChildCount(), 0u);
    ASSERT_EQ(c3->Get("key2"), 10.0);
    c3->SetEditable();
    ASSERT_FALSE(c3->IsActual());
    ASSERT_TRUE(c1->ChildInfo(kChildName3).IsValid());
    ASSERT_FALSE(top->ChildInfo(kChildName3).IsValid());
    ASSERT_FALSE(top->ChildInfo(kChildName3).IsActual());
    c3->Put("key1", "test");
    c3->Put("key2", 5.0);
    ASSERT_TRUE(c1->ChildInfo(kChildName3).IsActual());
    ASSERT_TRUE(c3->IsActual());
    ASSERT_EQ(c3->Get("key1"), std::string("test"));
    ASSERT_EQ(c3->Get("key2"), 5.0);
    c3->Save();
  }
  // Check data load of data which already has base and local data
  {
    auto session = dc::Session::Open(kWspFile2);
    auto top = session->OpenObject(kTopName2);
    auto c1 = top->OpenChild(kChildName1);
    ASSERT_EQ(top->ChildCount(), 2u);
    auto children = top->Children();
    ASSERT_TRUE(children[0].IsActual());
    ASSERT_FALSE(children[1].IsActual());
    ASSERT_TRUE(c1->IsActual());
    ASSERT_EQ(c1->Name(), std::string(kChildName1));
    ASSERT_EQ(c1->Path(), dc::DObjPath(fmt::format("{}/{}", kTopName2, kChildName1)));
    ASSERT_EQ(c1->Get("key1"), std::string("child1"));
    ASSERT_EQ(c1->ChildCount(), 1u);
    auto c3 = c1->OpenChild(kChildName3);
    ASSERT_EQ(c3->ChildCount(), 0u);
    ASSERT_EQ(c3->Get("key1"),std::string("test"));
    ASSERT_EQ(c3->Get("key2"), 5.0);
    auto c2 = top->OpenChild(kChildName2);
    ASSERT_FALSE(c2->IsActual());
    ASSERT_FALSE(top->ChildInfo(kChildName2).IsActual());
    c2->SetEditable();
    c2->Put("key1", 10);
    ASSERT_TRUE(c2->IsActual());
    ASSERT_TRUE(top->ChildInfo(kChildName2).IsActual());
    top->SetEditable();
    top->DeleteChild(kChildName2);
    ASSERT_FALSE(top->ChildInfo(kChildName2).IsActual());
    ASSERT_TRUE(top->HasChild(kChildName2));
    ASSERT_FALSE(top->HasActualChild(kChildName2));
    ASSERT_THROW(c2->Get("key1"), dc::DException);
    top->Save();
  }
  // Remove base
  {
    auto session = dc::Session::Open(kWspFile2);
    auto top = session->OpenObject(kTopName2);
    auto base = top->Bases()[0];
    auto c1 = top->OpenChild(kChildName1);
    ASSERT_EQ(top->Bases().size(), 1u);
    ASSERT_EQ(top->EffectiveBases().size(), 1u);
    ASSERT_EQ(c1->Bases().size(), 0u);
    ASSERT_EQ(c1->EffectiveBases().size(), 1u);
    top->SetEditable();
    top->RemoveBase(base);
    ASSERT_EQ(top->Bases().size(), 0u);
    ASSERT_EQ(top->EffectiveBases().size(), 0u);
    ASSERT_EQ(c1->Bases().size(), 0u);
    ASSERT_EQ(c1->EffectiveBases().size(), 0u);
    top->Save();
  }
  // Check data load after base removal
  {
    auto session = dc::Session::Open(kWspFile2);
    auto top = session->OpenObject(kTopName2);
    auto c1 = top->OpenChild(kChildName1);
    ASSERT_EQ(top->ChildCount(), 1u);
    ASSERT_TRUE(c1->IsActual());
    ASSERT_EQ(c1->Name(), std::string(kChildName1));
    ASSERT_EQ(c1->Path(), dc::DObjPath(fmt::format("{}/{}", kTopName2, kChildName1)));
    ASSERT_FALSE(c1->HasKey("key1"));
    ASSERT_EQ(c1->ChildCount(), 1u);
    auto c3 = c1->OpenChild(kChildName3);
    ASSERT_EQ(c3->ChildCount(), 0u);
    ASSERT_EQ(c3->Get("key1"), std::string("test"));
    ASSERT_EQ(c3->Get("key2"), 5.0);
  }
  // Need test to add base after creating some nodes
  {
    auto session = dc::Session::Open(kWspFile2);
    auto top1 = session->OpenObject(kTopName1);
    auto top2 = session->OpenObject(kTopName2, dc::OpenMode::kEditable);
    ASSERT_EQ(top2->ChildCount(), 1u);
    top2->AddBase(top1);
    ASSERT_EQ(top2->ChildCount(), 2u);
    ASSERT_TRUE(top2->ChildInfo(kChildName1).IsActual());
    ASSERT_FALSE(top2->ChildInfo(kChildName2).IsActual());
    auto c1 = top2->OpenChild(kChildName1);
    auto c3 = c1->OpenChild(kChildName3);
    ASSERT_TRUE(c3->IsActual());
    ASSERT_EQ(c3->Get("key2"), 5.0);
  }
  // create another tree (top3) with actual c1 node, then inherit
  {
    auto session = dc::Session::Open(kWspFile2);
    auto top = session->CreateTopLevelObject(kTopName3, kTopName3);
    session->InitTopLevelObjectPath(kTopName3, kTopName3);
    auto c1 = top->CreateChild(kChildName1, kChildName1);
    ASSERT_EQ(top->ChildCount(), 1u);
    ASSERT_TRUE(c1->IsActual());
    top->AddBase(session->OpenObject(kTopName1));
    ASSERT_EQ(top->ChildCount(), 2u);
    ASSERT_EQ(c1->ChildCount(), 1u);
    ASSERT_EQ(c1->Get("key1"), std::string("child1"));
    ASSERT_EQ(c1->Bases().size(), 0u);
    ASSERT_EQ(c1->EffectiveBases().size(), 1u);
    top->Save(true);
    session->Save();
  }
  // Reopen top3, test with child4 and child5
  // top1             : { "key1" : "value1", "key2" : 2, "key3" : nil }
  //   +- child1      : { "key1" : "child1" }
  //   |    +- child3 : { "key2" : 10.0 }
  //   |    +- child4 : { "key4" : "child4" }
  //   |    +- child5 : {}
  //   +- child2      : { "key3" : true }
  {
    auto session = dc::Session::Open(kWspFile2);
    auto top = session->OpenObject(kTopName3);
    auto c1 = top->OpenChild(kChildName1);
    ASSERT_EQ(top->ChildCount(), 2u);
    ASSERT_EQ(c1->ChildCount(), 1u);
    ASSERT_EQ(c1->Get("key1"), std::string("child1"));
    ASSERT_EQ(c1->EffectiveBases().size(), 1u);
    c1->SetEditable();
    auto c4 = c1->CreateChild(kChildName4, "test");
    ASSERT_TRUE(c4->IsActual());
    ASSERT_EQ(c4->EffectiveBases().size(), 0u);
    auto base_top = top->Bases()[0];
    auto base_c1 = base_top->GetChild(kChildName1, dc::OpenMode::kEditable);
    auto base_c4 = base_c1->CreateChild(kChildName4, "test");
    base_c4->Put("key4", "child4");
    ASSERT_EQ(c1->ChildCount(), 2u);
    ASSERT_EQ(base_c4->Get("key4"), std::string("child4"));
    ASSERT_TRUE(c4->HasKey("key4"));
    ASSERT_EQ(c4->Get("key4"), std::string("child4"));
    ASSERT_EQ(c4->EffectiveBases().size(), 1u);
    base_c1->DeleteChild(kChildName4);
    ASSERT_THROW(base_c4->Get("key4"), dc::DException);

    auto base_c5 = base_c1->CreateChild(kChildName5, "test");
    ASSERT_EQ(c1->ChildCount(), 3u);
    auto c5 = c1->OpenChild(kChildName5);
    ASSERT_FALSE(c5->IsActual());
    base_c1->DeleteChild(kChildName5);
    ASSERT_EQ(c1->ChildCount(), 2u);
    ASSERT_THROW(c5->IsActual(), dc::DException);
  }
}

TEST_F(InheritTest, DeepInheritance2) {
  // base object tree
  // top1                  : { "key1" : "value1", "key2" : 2, "key3" : nil }
  //   +- child1           : { "key1" : "child1" }
  //   |    +- child3      : { "key2" : 10.0 }
  //   |         +- child4 : {}
  //   |         +- child5 : {}
  //   +- child2           : { "key3" : true }

  // Create base tree
  {
    auto session = dc::Session::Create(kWspFile3);
    auto top = session->CreateTopLevelObject(kTopName1, kTopName1);
    session->InitTopLevelObjectPath(kTopName1, kTopName1);
    top->Put("key1", std::string("value1"));
    top->Put("key2", 2);
    top->Put("key3", nil);
    auto c1 = top->CreateChild(kChildName1, kChildName1);
    c1->Put("key1", std::string("child1"));
    auto c2 = top->CreateChild(kChildName2, kChildName2);
    c2->Put("key3", true);
    auto c3 = c1->CreateChild(kChildName3, kChildName3);
    c3->Put("key2", 10.0);
    auto c4 = c3->CreateChild(kChildName4, kChildName3);
    auto c5 = c3->CreateChild(kChildName5, kChildName5);
    top->Save();
    c1->Save();
    c2->Save();
    c3->Save();
    session->Save();
  }
  // Inherit and test
  {
    auto session = dc::Session::Open(kWspFile3);
    auto top = session->CreateTopLevelObject(kTopName2, kTopName2);
    session->InitTopLevelObjectPath(kTopName2, kTopName2);
    auto base_top = session->OpenObject(kTopName1);
    top->AddBase(base_top);
    ASSERT_EQ(top->ChildCount(), 2u);
    auto c1 = top->OpenChild(kChildName1);
    ASSERT_EQ(c1->ChildCount(), 1u);
    auto c3 = c1->OpenChild(kChildName3);
    ASSERT_EQ(c3->ChildCount(), 2u);
    top->RemoveBase(base_top);
    ASSERT_EQ(top->ChildCount(), 0u);
    ASSERT_THROW(c1->ChildCount(), dc::DException);
    ASSERT_THROW(c3->ChildCount(), dc::DException);
  }
}
