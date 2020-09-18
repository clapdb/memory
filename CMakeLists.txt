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
project(arena)
# logical target for all unit tests.
#add_custom_target (unit_tests)


### google test/mock
#include_directories(${BERNOULLI_SOURCE_DIR}/thirdparty/googletest/googletest)
#include_directories(${BERNOULLI_SOURCE_DIR}/thirdparty/googletest/googlemock)

aux_source_directory(. MEMORY_SRC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -frtti -fexceptions -fsized-deallocation")
set(COMMON_WARNING_FLAGS "${COMMON_WARNING_FLAGS} -Wall -Werror -Wextra -Wendif-labels -Wshadow")
set(CMAKE_CXX_EXTENSIONS OFF)


add_library(arena ${MEMORY_SRC})
target_link_libraries(arena glog)
set(ARENA_SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR})
add_subdirectory(test EXCLUDE_FROM_ALL)
