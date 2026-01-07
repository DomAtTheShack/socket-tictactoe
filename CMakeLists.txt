cmake_minimum_required(VERSION 3.28.3)
project(SocketTickTackToe)

set(CMAKE_CXX_STANDARD 20)

# 1. Fetch the library from GitHub automatically
include(FetchContent)
FetchContent_Declare(
        json
        URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)
FetchContent_MakeAvailable(json)

# 2. Define Server
add_executable(SocketTickTackToe server.cpp JsonSocket.h)
target_link_libraries(SocketTickTackToe PRIVATE nlohmann_json::nlohmann_json)

# 3. Define Client
add_executable(ClientTickTackToe client.cpp JsonSocket.h)
target_link_libraries(ClientTickTackToe PRIVATE nlohmann_json::nlohmann_json)