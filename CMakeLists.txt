cmake_minimum_required(VERSION 3.15)
project(coro LANGUAGES CXX)

include(CMakeDependentOption)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BUILD_TESTS "Build the tests, Default=ON." OFF)
option(BUILD_EXAMPLES "Build the examples, Default=ON." OFF)

cmake_dependent_option(NETWORKING "Include networking features, Default=ON." ON "NOT EMSCRIPTEN; NOT MSVC" OFF)

set(SOURCE_FILES
   src/sync_wait.cpp
   src/thread_pool.cpp
   src/poll.cpp
   src/io_scheduler.cpp
)

if (NETWORKING)
    list(APPEND SOURCE_FILES
        src/net/connect_status.cpp
        src/net/recv_status.cpp
        src/net/ip_address.cpp
        src/net/socket.cpp
        src/net/tcp/client.cpp
        src/net/tcp/server.cpp
        src/net/tcp/http/http_server.cpp
    )

endif()

add_library(${PROJECT_NAME} ${SOURCE_FILES})
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_BINARY_DIR}/include)

if (NETWORKING)
    target_compile_definitions(${PROJECT_NAME} PUBLIC NETWORKING)
endif()

if(BUILD_TESTS)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG release-1.12.1
    )
    
    # 设置 GTest 选项
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    
    FetchContent_MakeAvailable(googletest)
endif()

if (BUILD_TESTS)
    include(CTest)
    add_subdirectory(test)
endif()

if (BUILD_EXAMPLES)
    add_subdirectory(example)
endif()
