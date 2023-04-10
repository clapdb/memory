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
    if (APPLE)
        #set(CMAKE_C_COMPILER "/opt/homebrew/opt/llvm/bin/clang")
    elseif (UNIX)
        set(CMAKE_C_COMPILER "/usr/bin/clang")
    endif()
endif()

if(DEFINED ENV{CXX})
  set(CMAKE_CXX_COMPILER $ENV{CXX})
else()
    if (APPLE)
        #set(CMAKE_CXX_COMPILER "/opt/homebrew/opt/llvm/bin/clang++")
    elseif (UNIX)
        set(CMAKE_CXX_COMPILER "/usr/bin/clang++")
    endif()
endif()


project(memory)

aux_source_directory(. ARENA_SRC)

set(CMAKE_CXX_STANDARD 20)
set(COMMON_WARNING_FLAGS "${COMMON_WARNING_FLAGS} -Wall -Werror -Wextra -Wendif-labels -Wshadow -Wunused-parameter -Wunused-variable  -Wparentheses -Wuninitialized -Wcomment -Wformat -Wimplicit-fallthrough -Wsign-conversion")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COMMON_WARNING_FLAGS} -frtti -fexceptions -fsized-deallocation")
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# sanitizer
set(SANITIZE_PARAM "-fsanitize=address -fsanitize=undefined -fno-sanitize=vptr -fno-omit-frame-pointer")  # param from seastar.pc
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ${SANITIZE_PARAM}")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} ${SANITIZE_PARAM}")
set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} ${SANITIZE_PARAM}")
set(CMAKE_MODULE_LINKER_FLAGS_DEBUG "${CMAKE_MODULE_LINKER_FLAGS_DEBUG} ${SANITIZE_PARAM}")

list(APPEND TEST_FLAGS
        -D__UNITTEST)

OPTION(CODE_COVERAGE
       "build with code coverage, only work in DEBUG"
        OFF
        )
# fmt
find_package(fmt REQUIRED)
# xxhash
find_package(xxHash REQUIRED)
# boost
find_package(Boost REQUIRED)
include_directories(SYSTEM ${Boost_INCLUDE_DIRS})

# pmr for apple Mac OSX
find_library(lib_cxxexperimental c++experimental PATHS /opt/homebrew/opt/llvm/lib/c++)
if(lib_cxxexperimental MATCHES "NOTFOUND")
    message (STATUS "libc++experimental NOTFOUND")
else()
    message (STATUS "${lib_cxxexperimental}")
endif()

set(MEMORY_SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR})
include_directories(SYSTEM ${MEMORY_SOURCE_DIR}/doctest/)
include_directories(${MEMORY_SOURCE_DIR}/)

## code coverage

# code coverage section **must** place before all subdirectories to test coverage.
if((${CMAKE_BUILD_TYPE} MATCHES Debug) AND CODE_COVERAGE)
  include(CodeCoverage)
  append_coverage_compiler_flags()

  set(COVERAGE_EXCLUDES
    "doctest/*"
    "test/*"
    "build/*"
    "build.debug/*"
    "build_debug/*"
    "cmake-build-*"
    "/usr/*")

  set(GCOVR_ADDITIONAL_ARGS
    "--print-summary"
  )

  setup_target_for_coverage_gcovr_html(
    NAME coverage
    EXECUTABLE test/memory_test
  )
endif()

### build arena
add_subdirectory(arena)
add_subdirectory(optparse)

add_subdirectory(doctest EXCLUE_FROM_ALL)
add_subdirectory(test EXCLUDE_FROM_ALL)
add_subdirectory(benchmark EXCLUDE_FROM_ALL)

