cmake_minimum_required(VERSION 3.15)
project(coro_examples)

add_executable(example_task example_task.cpp)
target_link_libraries(example_task PUBLIC coro)

add_executable(example_when_all example_when_all.cpp)
target_link_libraries(example_when_all PUBLIC coro)

add_executable(example_thread_pool example_thread_pool.cpp)
target_link_libraries(example_thread_pool PUBLIC coro)

if (NETWORKING)
    add_executable(tcp_echo_server tcp_echo_server.cpp)
    target_link_libraries(tcp_echo_server PUBLIC coro)

    add_executable(http_200_ok_server http_200_ok_server.cpp)
    target_link_libraries(http_200_ok_server PUBLIC coro)

    add_executable(example_http_server example_http_server.cpp)
    target_link_libraries(example_http_server PUBLIC coro)

endif()

