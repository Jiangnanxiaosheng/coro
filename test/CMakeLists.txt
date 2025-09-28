cmake_minimum_required(VERSION 3.15)
project(test_for_coro)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(GTest REQUIRED)

set(TEST_SOURCE_FILES
    test_task.cpp
    test_sync_wait.cpp
)

add_executable(test_memory benchmark/test_memory.cpp)
target_include_directories(test_memory PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(test_memory PRIVATE coro)


add_executable(${PROJECT_NAME} main.cpp ${TEST_SOURCE_FILES})
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(${PROJECT_NAME} PRIVATE coro gtest gtest_main)

add_test(NAME coro_tests COMMAND ${PROJECT_NAME})

enable_testing()