cmake_minimum_required(VERSION 3.27)
project(docgen)

set(CMAKE_CXX_STANDARD 17)

add_executable(docgen main.cpp
        glob.hpp)
