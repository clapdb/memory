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

aux_source_directory(. ARENA_SRC)

set(CMAKE_CXX_STANDARD 20)
set(COMMON_WARNING_FLAGS "${COMMON_WARNING_FLAGS} -Wall -Werror -Wextra -Wendif-labels -Wshadow -Wunused-result -Wparentheses -Wuninitialized -Wcomment -Wformat")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COMMON_WARNING_FLAGS} -frtti -fexceptions -fsized-deallocation")
set(CMAKE_CXX_EXTENSIONS ON)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

include (cmake/FindGTest.cmake)
include (cmake/FindGMock.cmake)

find_package(GTest REQUIRED)
find_package(GMock REQUIRED)

add_library(arena ${ARENA_SRC})
target_link_libraries(arena glog fmt)
set(ARENA_SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR})

if(${CMAKE_BUILD_TYPE} MATCHES Debug)
  include(CodeCoverage)
  append_coverage_compiler_flags()

  set(COVERAGE_EXCLUDES
    "test/*"
    "/usr/*")

  setup_target_for_coverage_gcovr_html(
    NAME coverage
    EXECUTABLE test/test
  )
endif()

add_subdirectory(test)
