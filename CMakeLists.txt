cmake_minimum_required(VERSION 3.16)
project(libhv_test)

include_directories("/usr/local/include/hv")
link_directories("/usr/local/lib")
link_libraries(hv)

set(CMAKE_CXX_STANDARD 17)

add_executable(libhv_test main.cpp)