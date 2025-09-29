cmake_minimum_required(VERSION 3.15)
project(coro LANGUAGES CXX)

include(CMakeDependentOption)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BUILD_TESTS "Build the tests, Default=ON." ON)
option(BUILD_EXAMPLES "Build the examples, Default=ON." ON)

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
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/third_party/googletest/CMakeLists.txt)
        add_subdirectory(third_party/googletest)
    else()
        message(WARNING "GTest source not found in third_party/googletest, tests will be disabled")
        set(BUILD_TESTS OFF)
    endif()
endif()

if (BUILD_TESTS)
    include(CTest)
    add_subdirectory(test)
endif()

if (BUILD_EXAMPLES)
    add_subdirectory(example)
endif()
