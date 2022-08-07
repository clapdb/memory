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
# position matters, must define before project
if(DEFINED ENV{CC})
  set(CMAKE_C_COMPILER $ENV{CC})
else()
  set(CMAKE_C_COMPILER "/usr/bin/clang")
endif()

if(DEFINED ENV{CXX})
  set(CMAKE_CXX_COMPILER $ENV{CXX})
else()
  set(CMAKE_CXX_COMPILER "/usr/bin/clang++")
endif()


project(arena)

aux_source_directory(. ARENA_SRC)

set(CMAKE_CXX_STANDARD 20)
set(COMMON_WARNING_FLAGS "${COMMON_WARNING_FLAGS} -Wall -Werror -Wextra -Wendif-labels -Wshadow -Wunused-parameter -Wunused-variable  -Wparentheses -Wuninitialized -Wcomment -Wformat -Wimplicit-fallthrough -Wsign-conversion")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COMMON_WARNING_FLAGS} -frtti -fexceptions -fsized-deallocation")
set(CMAKE_CXX_EXTENSIONS ON)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

include_directories(SYSTEM doctest/)

# fmt
find_package(fmt REQUIRED)

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

add_subdirectory(doctest)

### build arena
add_subdirectory(arena)

add_subdirectory(test)
