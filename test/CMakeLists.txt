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

add_executable(memory_test EXCLUDE_FROM_ALL ${TEST_SRC})

add_executable(map_test EXCLUDE_FROM_ALL map_test.cc test_main.cc)

## test flags
list(APPEND TEST_FLAGS
  #-DBOOST_STACKTRACE_USE_BACKTRACE
  -DDOCTEST_CONFIG_SUPER_FAST_ASSERTS
  -Wno-deprecated
)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated-declarations -Wno-error=cpp")
### include arena src
#target_include_directories(memory_test PRIVATE ${MEMORY_SOURCE_DIR}/)

target_compile_options(memory_test PRIVATE ${TEST_FLAGS})
target_compile_options(map_test PRIVATE ${TEST_FLAGS})
if (APPLE)
    link_directories(/opt/homebrew/opt/llvm/lib/c++)
    target_link_libraries(memory_test arena dl doctest xxHash::xxHash optparse ${lib_cxxexperimental} ${TEST_LINK_LIBS})
    target_link_libraries(map_test arena dl doctest xxHash::xxHash optparse ${lib_cxxexperimental} ${TEST_LINK_LIBS})
elseif (UNIX)
    target_link_libraries(map_test dl doctest xxHash::xxHash)
    target_link_libraries(memory_test arena dl doctest xxHash::xxHash optparse ${TEST_LINK_LIBS})
endif()
