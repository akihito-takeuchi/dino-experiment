macro(dinoAddTest test_name src_path)
  add_executable("${test_name}" "${src_path}")
  target_link_libraries("${test_name}" ${GTEST_BOTH_LIBRARIES} dino_core ${Boost_LIBRARIES} fmt pthread)
  add_test(NAME "${test_name}"
    COMMAND $<TARGET_FILE:${test_name}>)
endmacro()

dinoAddTest("DataIOTest" "dataio_test.cc")
dinoAddTest("ObjectTest" "object_test.cc")
dinoAddTest("InheritTest" "inherit_test.cc")
dinoAddTest("FlattenedInheritTest" "flattenedinherit_test.cc")
dinoAddTest("ObjectFactoryTest" "objectfactory_test.cc")
dinoAddTest("DValueTest" "dvalue_test.cc")
dinoAddTest("AttributeTest" "attribute_test.cc")
