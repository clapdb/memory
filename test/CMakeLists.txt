#[[
 Copyright (C) 2020 hurricane <l@stdb.io>. All rights reserved.
  +------------------------------------------------------------------+
   |                                                                  |
   |                                                                  |
   |  #####                                                           |
   | #     # #####   ##   #####  #####  # #    # ######  ####   ####  |
   | #         #    #  #  #    # #    # # ##   # #      #      #      |
   |  #####    #   #    # #    # #    # # # #  # #####   ####   ####  |
   |       #   #   ###### #####  #####  # #  # # #           #      # |
   | #     #   #   #    # #   #  #   #  # #   ## #      #    # #    # |
   |  #####    #   #    # #    # #    # # #    # ######  ####   ####  |
   |                                                                  |
   |                                                                  |
   +------------------------------------------------------------------+
 ]]

cmake_minimum_required(VERSION 3.10)

aux_source_directory(. TEST_SRC)

add_executable(test EXCLUDE_FROM_ALL ${TEST_SRC})

### include arena src
target_include_directories(test PRIVATE ${ARENA_SOURCE_DIR}/)

if (${GTEST_FOUND} AND ${GMOCK_FOUND})

target_include_directories(test PRIVATE ${GTEST_INCLUDE_DIRS})
target_include_directories(test PRIVATE ${GMOCK_INCLUDE_DIRS})

set(TEST_FLAGS "-D__UNITTEST -DSTAR_ENABLE_FORCE_INLINE -D_MULTI_THREAD_")

target_compile_options(test PRIVATE ${TEST_FLAGS})

target_link_libraries(test glog arena GTest::GTest GTest::Main ${GMOCK_LIBRARIES})

else()
  message(FATAL_ERROR "no gtest/gmock found, cannot run test cases.")
endif()

