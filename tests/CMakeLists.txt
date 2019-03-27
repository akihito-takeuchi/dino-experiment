macro(dinoAddTest test_name src_path)
  add_executable("${test_name}" "${src_path}")
  target_link_libraries("${test_name}" gtest gtest_main dino_core ${Boost_LIBRARIES} fmt gtest gtest_main pthread)
  add_test(NAME "${test_name}"
    COMMAND $<TARGET_FILE:${test_name}>)
endmacro()

dinoAddTest("DataIOTest" "dataio_test.cc")
dinoAddTest("ObjectTest" "object_test.cc")
dinoAddTest("InheritTest" "inherit_test.cc")
dinoAddTest("ObjectFactoryTest" "objectfactory_test.cc")
