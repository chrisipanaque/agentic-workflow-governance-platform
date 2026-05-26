cmake_minimum_required(VERSION 3.20)
project(ai-control-plane LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

add_executable(ai-control-plane src/main.cpp)
