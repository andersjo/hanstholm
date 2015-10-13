cmake_minimum_required(VERSION 2.8.4)
project(HANSTHOLM)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall -O0")

set(SOURCE_FILES src/features.cc
    src/input.cc src/learn.cc src/parse.cc
    src/hashtable_block.cc
    src/output.cc src/output.cc
    src/feature_set_parser.cc
    src/feature_combiner.cc
    src/feature_handling.h
    )

# Build library as advised by
# http://stackoverflow.com/questions/14446495/cmake-project-structure-with-unit-tests
add_library (libhanstholm ${SOURCE_FILES})

option(HANSTHOLM_BUILD_TESTS "Build Hanstholm tests" OFF)

# set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall")


# set(Boost_USE_MULTI_THREADED OFF)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/lib)
set(BOOST_LIBRARYDIR "${BOOST_ROOT}/lib")

find_package(Boost 1.36.0 REQUIRED system program_options)
if(Boost_FOUND)
  message("Boost is found")
  include_directories(${Boost_INCLUDE_DIRS})
  add_executable(hanstholm src/main.cpp)
  message("Boost_LIBRARIES is ${Boost_LIBRARIES}")
  target_link_libraries(hanstholm ${Boost_LIBRARIES} libhanstholm)
endif()

if(HANSTHOLM_BUILD_TESTS)
    add_subdirectory(test)
endif()
