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
add_subdirectory(googletest)

add_executable(test EXCLUDE_FROM_ALL ${TEST_SRC})
### include arena src
target_include_directories(test PRIVATE ${ARENA_SOURCE_DIR}/)

### google test/mock
target_include_directories(test PRIVATE ${TEST_SRC}/googletest/googletest/include)
target_include_directories(test PRIVATE ${TEST_SRC}/googletest/googlemock/include)

set(TEST_FLAGS "-D__UNITTEST -DSTAR_ENABLE_FORCE_INLINE -D_MULTI_THREAD_")

target_compile_options(test PRIVATE ${TEST_FLAGS})

target_link_libraries(test gtest gtest_main gmock glog arena)
