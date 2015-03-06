cmake_minimum_required(VERSION 2.8.4)
project(hanstholm)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall")

set(SOURCE_FILES src/main.cpp src/features.cc
    src/input.cc src/learn.cc src/parse.cc
    src/hashtable_block.cc
    src/output.cc src/output.cc
    src/feature_set_parser.cc
    )
add_executable(hanstholm ${SOURCE_FILES})