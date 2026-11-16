cmake_minimum_required(VERSION 3.16)
project(CafeTokenManagement LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Optimization and warnings
if(MSVC)
    add_compile_options(/W4 /EHsc)
else()
    add_compile_options(-Wall -Wextra -O2)
endif()

# Source files
set(SOURCES
    backend/main.cpp
    backend/database/Database.cpp
    backend/order/OrderManager.cpp
    backend/server/HttpServer.cpp
    backend/third_party/sqlite3.c
)

# Header include paths
include_directories(
    backend
    backend/third_party
    backend/structures
    backend/order
    backend/database
    backend/server
)

# Executable target
add_executable(CafeServer ${SOURCES})

# Windows socket and crypto libraries required by cpp-httplib
if(WIN32)
    target_link_libraries(CafeServer PRIVATE ws2_32 crypt32)
endif()

# Test target for manual data structures validation
add_executable(test_structures
    tests/test_structures.cpp
)
