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

## test flags
list(APPEND TEST_FLAGS
  -DBOOST_STACKTRACE_USE_BACKTRACE
  -DDOCTEST_CONFIG_SUPER_FAST_ASSERTS
)

### include arena src
target_include_directories(test PRIVATE ${ARENA_SOURCE_DIR}/)

target_compile_options(test PRIVATE ${TEST_FLAGS})
target_link_libraries(test arena doctest)
