cmake_minimum_required(VERSION 3.8)

project(co2monpp)

add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")

add_subdirectory(hidapi)
add_subdirectory(logger)
