cmake_minimum_required(VERSION 3.1)
project(libhv_test)

include_directories("/usr/local/include/hv")
link_directories("/usr/local/lib")
link_libraries(hv)

set(CMAKE_CXX_STANDARD 11)

file(MAKE_DIRECTORY output)

SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/output)

add_executable(libhv_test main.cpp)