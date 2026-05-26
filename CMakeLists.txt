cmake_minimum_required(VERSION 3.20)
project(ai-control-plane LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

include(FetchContent)

FetchContent_Declare(
    nlohmann_json
    URL https://github.com/nlohmann/json/archive/refs/tags/v3.11.2.tar.gz
)
FetchContent_MakeAvailable(nlohmann_json)

add_executable(ai-control-plane 
    src/main.cpp
    src/config_loader.cpp
)

target_include_directories(ai-control-plane PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(ai-control-plane PRIVATE nlohmann_json::nlohmann_json)
