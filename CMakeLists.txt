cmake_minimum_required(VERSION 3.23)
project(cpp_advanced_vector)

set(CMAKE_CXX_STANDARD 17)

include_directories(advanced-vector)

add_executable(cpp_advanced_vector
        advanced-vector/main.cpp
        advanced-vector/vector.h)
