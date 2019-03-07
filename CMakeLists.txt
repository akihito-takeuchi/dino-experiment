#
# DINO project
#

project(dino)

cmake_minimum_required(VERSION 3.4)

# Common compile setup
include(CheckCXXCompilerFlag)
check_cxx_compiler_flag("-Wno-class-memaccess" cxx-flag-class-memaccess)
if(cxx-flag-class-memaccess)
  set(CMAKE_CXX_FLAGS "-Wall -W -DDINO_ROOT=${CMAKE_CURRENT_LIST_DIR} -Wno-class-memaccess")
else()
  set(CMAKE_CXX_FLAGS "-Wall -W -DDINO_ROOT=${CMAKE_CURRENT_LIST_DIR}")
endif()

set(CMAKE_CXX_STANDARD 14)
if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Setup boost
find_package(Boost
  1.67 REQUIRED
  COMPONENTS system filesystem)
include_directories(${Boost_INCLUDE_DIRS})

# Setup fmtlib
set(BUILD_SHARED_LIBS true)
set(FMT_ROOT "${CMAKE_CURRENT_LIST_DIR}/third_party/fmt-5.2.1")
add_subdirectory(${FMT_ROOT})
include_directories(${FMT_ROOT}/include)

# Setup rapidjson
set(RAPIDJSON_ROOT "${CMAKE_CURRENT_LIST_DIR}/third_party/rapidjson")
include_directories(${RAPIDJSON_ROOT}/include)

# Setup spdlog
set(SPDLOG_ROOT "${CMAKE_CURRENT_LIST_DIR}/third_party/spdlog")
include_directories(${SPDLOG_ROOT}/include)

# Dino
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
add_subdirectory("dino/core")

# For test
set(with-test true)

# unit tests
if(with-test)
  enable_testing()
  include_directories("${GTEST_ROOT}/include")
  add_subdirectory(tests)
endif(with-test)
