cmake_minimum_required(VERSION 3.19)
project(os_proj2)

set(CMAKE_CXX_STANDARD 11)

add_executable(main mcheck_test.c)
add_executable(top top/top_test.cpp)
add_executable(example_cpp testcase/mem_leak_example.cpp)