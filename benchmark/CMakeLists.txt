#[[
 Copyright (C) 2020 hurricane <l@stdb.io>. All rights reserved.
 +------------------------------------------------------------------------------+
 |                                                                              |
 |                                                                              |
 |                    ..######..########.########..########.                    |
 |                    .##....##....##....##.....##.##.....##                    |
 |                    .##..........##....##.....##.##.....##                    |
 |                    ..######.....##....##.....##.########.                    |
 |                    .......##....##....##.....##.##.....##                    |
 |                    .##....##....##....##.....##.##.....##                    |
 |                    ..######.....##....########..########.                    |
 |                                                                              |
 |                                                                              |
 |                                                                              |
 +------------------------------------------------------------------------------+
]]

# Use nanobench from git submodule
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../nanobench ${CMAKE_CURRENT_BINARY_DIR}/nanobench)

#FILE(GLOB_RECURSE BENCH_SRC *.cc)
LIST(APPEND BENCH_SRC vector_bench.cc)
add_executable(vb ${BENCH_SRC})

# Link with nanobench (header-only, so just need include)
target_link_libraries(vb nanobench)

