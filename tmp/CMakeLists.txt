cmake_minimum_required(VERSION 3.16)
project(readstat_scratch C)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(test_bin main.c)

# Import the readstat shared library built by the parent extension.
# Because the full path is known, CMake adds ../build/release to the
# build RPATH, so test_bin runs without setting LD_LIBRARY_PATH.
add_library(read_stat SHARED IMPORTED)
set_target_properties(read_stat PROPERTIES
    IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/../build/release/libread_stat.so"
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/readstat/src")

# m: readstat uses math functions such as fmod
target_link_libraries(test_bin PRIVATE read_stat m)
