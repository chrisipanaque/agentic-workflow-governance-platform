cmake_minimum_required(VERSION 3.20)
project(ai-governance-platform LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

include(FetchContent)

FetchContent_Declare(
    nlohmann_json
    URL https://github.com/nlohmann/json/archive/refs/tags/v3.11.2.tar.gz
)
FetchContent_MakeAvailable(nlohmann_json)

add_executable(ai-governance-platform 
    src/main.cpp
    src/config_loader.cpp
    src/diff_scanner.cpp
    src/path_validator.cpp
    src/risk_engine.cpp
    src/trace_logger.cpp
    src/audit_log.cpp
    src/github_metadata.cpp
    src/approval_gate.cpp
    src/dependency_validator.cpp
    src/ownership_validator.cpp
)

target_include_directories(ai-governance-platform PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(ai-governance-platform PRIVATE nlohmann_json::nlohmann_json)
