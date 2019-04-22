cmake_minimum_required(VERSION 2.8)
project(os-jit)

set(CMAKE_CXX_STANDARD 14)
set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address,undefined")

add_executable(jit jit.cpp sum.cpp)