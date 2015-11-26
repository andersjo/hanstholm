set(SOURCE_FILES test_main.cc feature_handling.cc constraints.cc nonproj.cc)

include_directories(${HANSTHOLM_SOURCE_DIR}/src)

add_executable(hanstholm_test ${SOURCE_FILES})
target_link_libraries(hanstholm_test ${Boost_LIBRARIES} libhanstholm)