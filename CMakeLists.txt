cmake_minimum_required(VERSION 2.8.4)
project(hanstholm)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(SOURCE_FILES src/main.cpp src/features.cc src/features.h src/hash.h src/input.cc src/learn.h src/learn.cc src/parse.cc src/parse.h)
add_executable(hanstholm ${SOURCE_FILES})