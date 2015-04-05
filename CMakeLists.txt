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


include_directories(${CMAKE_CURRENT_SOURCE_DIR}/lib)
set(BOOST_LIBRARYDIR "${BOOST_ROOT}/lib")


find_package(Boost 1.36.0 REQUIRED system program_options)
if(Boost_FOUND)
  message("Boost is found")
  include_directories(${Boost_INCLUDE_DIRS})
  add_executable(hanstholm ${SOURCE_FILES})
  message("Boost_LIBRARIES is ${Boost_LIBRARIES}")
  target_link_libraries(hanstholm ${Boost_LIBRARIES})
endif()