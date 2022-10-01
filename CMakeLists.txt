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


project(memory)

aux_source_directory(. ARENA_SRC)

set(CMAKE_CXX_STANDARD 20)
set(COMMON_WARNING_FLAGS "${COMMON_WARNING_FLAGS} -Wall -Werror -Wextra -Wendif-labels -Wshadow -Wunused-parameter -Wunused-variable  -Wparentheses -Wuninitialized -Wcomment -Wformat -Wimplicit-fallthrough -Wsign-conversion")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COMMON_WARNING_FLAGS} -frtti -fexceptions -fsized-deallocation")
set(CMAKE_CXX_EXTENSIONS ON)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# sanitizer
set(SANITIZE_PARAM "-fsanitize=address -fsanitize=undefined -fno-sanitize=vptr -fno-omit-frame-pointer")  # param from seastar.pc
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ${SANITIZE_PARAM}")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} ${SANITIZE_PARAM}")
set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} ${SANITIZE_PARAM}")
set(CMAKE_MODULE_LINKER_FLAGS_DEBUG "${CMAKE_MODULE_LINKER_FLAGS_DEBUG} ${SANITIZE_PARAM}")

# fmt
find_package(fmt REQUIRED)

set(ARENA_SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR})
include_directories(SYSTEM ${ARENA_SOURCE_DIR}/doctest/)
include_directories(${ARENA_SOURCE_DIR}/)

### build arena
add_subdirectory(arena)

add_subdirectory(doctest EXCLUE_FROM_ALL)
add_subdirectory(test EXCLUDE_FROM_ALL)
