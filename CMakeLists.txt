cmake_minimum_required(VERSION 3.22)

project(pyasm)

set(CMAKE_CXX_STANDARD 20)

add_executable(pyasm
        src/main.cpp
        src/lexer.h)