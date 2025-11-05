cmake_minimum_required(VERSION 3.10)
project(MiniRedis)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JSON library
find_package(nlohmann_json REQUIRED)

include_directories(src/headers)

# SERVER
add_executable(server
        src/server.cpp
        src/Buffer.cpp
        src/DLL.cpp
        src/HashTable.cpp
        src/TTLHeap.cpp
        src/UtilFuncs.cpp
)

# CLIENT
add_executable(client
        src/Client.cpp
        src/Buffer.cpp
        src/DLL.cpp
        src/HashTable.cpp
        src/TTLHeap.cpp
        src/UtilFuncs.cpp
)

target_link_libraries(server PRIVATE nlohmann_json::nlohmann_json)
target_link_libraries(client PRIVATE nlohmann_json::nlohmann_json)
