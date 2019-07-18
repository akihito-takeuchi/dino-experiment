#
# DINO project
#

project(dino)

cmake_minimum_required(VERSION 3.4)
include(GNUInstallDirs)

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
set(FMT_INSTALL true)
add_subdirectory(${FMT_ROOT})

# Setup rapidjson
set(RAPIDJSON_ROOT "${CMAKE_CURRENT_LIST_DIR}/third_party/rapidjson")
include_directories(${RAPIDJSON_ROOT}/include)

# Dino
add_subdirectory("src")

set(enable-qt true)

if(enable-qt)
  set(QTDIR $ENV{QTDIR})
  set(CMAKE_PREFIX_PATH ${QTDIR})
  set(CMAKE_AUTOMOC on)
  set(CMAKE_INCLUDE_CURRENT_DIR on)
  find_package(Qt5Core REQUIRED)
  find_package(Qt5Widgets REQUIRED)
  add_subdirectory("src/dino/qt")
endif(enable-qt)

# For test
set(with-test true)

# unit tests
if(with-test)
  find_package(GTest REQUIRED)
  enable_testing()
  include_directories(${GTEST_INCLUDE_DIRS})
  add_subdirectory(tests)
endif(with-test)
