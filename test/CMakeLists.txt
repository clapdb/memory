#
# license is here
#

cmake_minimum_required(VERSION 3.10)

aux_source_directory(. TEST_SRC)

add_executable(tests ${TEST_SRC})
### include arena src
target_include_directories(tests PRIVATE ${ARENA_SOURCE_DIR}/)

### google test/mock
target_include_directories(tests PRIVATE ${ARENA_SOURCE_DIR}/googletest/googletest/include)
target_include_directories(tests PRIVATE ${ARENA_SOURCE_DIR}/googletest/googlemock/include)

set(TEST_FLAGS "-D__UNITTEST -DSTAR_ENABLE_FORCE_INLINE -D_MULTI_THREAD_")

target_compile_options(tests PRIVATE ${TEST_FLAGS})

target_link_libraries(tests gtest gtest_main gmock glog arena)
