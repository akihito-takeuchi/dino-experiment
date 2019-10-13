// Copyright (c) 2019 Akihito Takeuchi
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

class FlattenedInheritTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    for (auto dir_name
             : {kTopName1, kTopName2, kTopName3, kTopName4})
      if (fs::exists(dir_name))
        fs::remove_all(dir_name);
  }
};

TEST_F(FlattenedInheritTest, DeepInheritance) {
  // base object tree
  // top1             : { "key1" : "value1", "key2" : 2, "key3" : nil }
  //   +- child1      : { "key1" : "child1" }
  //   |    +- child3 : { "key2" : 10.0 }
  //   +- child2      : { "key3" : true }

  // Create base tree
  {
    auto session = dc::Session::Create();
    auto top = session->CreateTopLevelObject(kTopName1, kTopName1);
    session->InitTopLevelObjectPath(kTopName1, kTopName1);
    top->Put("key1", std::string("value1"));
    top->Put("key2", 2);
    top->Put("key3", nil);
    auto c1 = top->CreateChild(kChildName1, kChildName1, true);
    c1->Put("key1", std::string("child1"));
    auto c2 = top->CreateChild(kChildName2, kChildName2, true);
    c2->Put("key3", true);
    auto c3 = c1->CreateChild(kChildName3, kChildName3, true);
    c3->Put("key2", 10.0);
    
    top->Save();
  }
  // Test base tree
  {
    auto session = dc::Session::Create();
    auto top = session->OpenTopLevelObject(kTopName1, kTopName1);
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
    auto session = dc::Session::Create();
    auto top = session->CreateTopLevelObject(kTopName2, kTopName2);
    session->InitTopLevelObjectPath(kTopName2, kTopName2);
    top->AddBase(session->OpenTopLevelObject(kTopName1, kTopName1));
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
  }
  // Check inherited, and put data&save
  {
    auto session = dc::Session::Create();
    // Need to open base object first
    session->OpenTopLevelObject(kTopName1, kTopName1);
    auto top = session->OpenTopLevelObject(kTopName2, kTopName2);
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
    top->SetEditable();
    top->Save(true);
  }
  // Check data load of data which already has base and local data
  {
    auto session = dc::Session::Create();
    // Need to open base object first
    session->OpenTopLevelObject(kTopName1, kTopName1);
    auto top = session->OpenTopLevelObject(kTopName2, kTopName2);
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
    top->Save(true);
  }
  // Remove base
  {
    auto session = dc::Session::Create();
    // Need to open base object first
    session->OpenTopLevelObject(kTopName1, kTopName1);
    auto top = session->OpenTopLevelObject(kTopName2, kTopName2);
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
    top->Save(true);
  }
  // Check data load after base removal
  {
    auto session = dc::Session::Create();
    auto top = session->OpenTopLevelObject(kTopName2, kTopName2);
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
    auto session = dc::Session::Create();
    auto top1 = session->OpenTopLevelObject(kTopName1, kTopName1);
    auto top2 = session->OpenTopLevelObject(
        kTopName2, kTopName2, dc::OpenMode::kEditable);
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
    auto session = dc::Session::Create();
    auto top = session->CreateTopLevelObject(kTopName3, kTopName3);
    session->InitTopLevelObjectPath(kTopName3, kTopName3);
    auto c1 = top->CreateChild(kChildName1, kChildName1, true);
    ASSERT_EQ(top->ChildCount(), 1u);
    ASSERT_TRUE(c1->IsActual());
    top->AddBase(session->OpenTopLevelObject(kTopName1, kTopName1));
    ASSERT_EQ(top->ChildCount(), 2u);
    ASSERT_EQ(c1->ChildCount(), 1u);
    ASSERT_EQ(c1->Get("key1"), std::string("child1"));
    ASSERT_EQ(c1->Bases().size(), 0u);
    ASSERT_EQ(c1->EffectiveBases().size(), 1u);
    top->Save(true);
  }
  #if 0
  // Reopen top3, test with child4 and child5
  // top1             : { "key1" : "value1", "key2" : 2, "key3" : nil }
  //   +- child1      : { "key1" : "child1" }
  //   |    +- child3 : { "key2" : 10.0 }
  //   |    +- child4 : { "key4" : "child4" }
  //   |    +- child5 : {}
  //   +- child2      : { "key3" : true }
  {
    auto session = dc::Session::Create();
    // Need to open base object first
    session->OpenTopLevelObject(kTopName1, kTopName1);
    auto top = session->OpenTopLevelObject(kTopName3, kTopName3);
    auto c1 = top->OpenChild(kChildName1);
    ASSERT_EQ(top->ChildCount(), 2u);
    ASSERT_EQ(c1->ChildCount(), 1u);
    ASSERT_EQ(c1->Get("key1"), std::string("child1"));
    ASSERT_EQ(c1->EffectiveBases().size(), 1u);
    c1->SetEditable();
    auto c4 = c1->CreateChild(kChildName4, "test", true);
    ASSERT_TRUE(c4->IsActual());
    ASSERT_EQ(c4->EffectiveBases().size(), 0u);
    auto base_top = top->Bases()[0];
    auto base_c1 = base_top->GetChild(kChildName1, dc::OpenMode::kEditable);
    auto base_c4 = base_c1->CreateChild(kChildName4, "test", true);
    base_c4->Put("key4", "child4");
    ASSERT_EQ(c1->ChildCount(), 2u);
    ASSERT_EQ(base_c4->Get("key4"), std::string("child4"));
    ASSERT_TRUE(c4->HasKey("key4"));
    ASSERT_EQ(c4->Get("key4"), std::string("child4"));
    ASSERT_EQ(c4->EffectiveBases().size(), 1u);
    base_c1->DeleteChild(kChildName4);
    ASSERT_THROW(base_c4->Get("key4"), dc::DException);

    auto base_c5 = base_c1->CreateChild(kChildName5, "test", true);
    ASSERT_EQ(c1->ChildCount(), 3u);
    auto c5 = c1->OpenChild(kChildName5);
    ASSERT_FALSE(c5->IsActual());
    base_c1->DeleteChild(kChildName5);
    ASSERT_EQ(c1->ChildCount(), 2u);
    ASSERT_THROW(c5->IsActual(), dc::DException);
  }
  #endif
}

#if 0

TEST_F(FlattenedInheritTest, DeepInheritance2) {
  // base object tree
  // top1                  : { "key1" : "value1", "key2" : 2, "key3" : nil }
  //   +- child1           : { "key1" : "child1" }
  //   |    +- child3      : { "key2" : 10.0 }
  //   |         +- child4 : {}
  //   |         +- child5 : {}
  //   +- child2           : { "key3" : true }

  // Create base tree
  {
    auto session = dc::Session::Create();
    auto top = session->CreateTopLevelObject(kTopName1, kTopName1);
    session->InitTopLevelObjectPath(kTopName1, kTopName1);
    top->Put("key1", std::string("value1"));
    top->Put("key2", 2);
    top->Put("key3", nil);
    auto c1 = top->CreateChild(kChildName1, kChildName1, true);
    c1->Put("key1", std::string("child1"));
    auto c2 = top->CreateChild(kChildName2, kChildName2, true);
    c2->Put("key3", true);
    auto c3 = c1->CreateChild(kChildName3, kChildName3, true);
    c3->Put("key2", 10.0);
    auto c4 = c3->CreateChild(kChildName4, kChildName3, true);
    auto c5 = c3->CreateChild(kChildName5, kChildName5, true);
    top->Save();
  }
  // Inherit and test
  {
    auto session = dc::Session::Create();
    auto top = session->CreateTopLevelObject(kTopName2, kTopName2);
    session->InitTopLevelObjectPath(kTopName2, kTopName2);
    auto base_top = session->OpenTopLevelObject(kTopName1, kTopName1);
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
#endif
