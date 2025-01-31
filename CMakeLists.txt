cmake_minimum_required(VERSION 3.20)

project(openmp_webserver LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT UNIX)
    message(FATAL_ERROR "This implementation requires Linux/WSL (epoll).")
endif()

add_executable(openmp_webserver
    src/main.cpp
    src/thread_pool.cpp
    src/epoll_server.cpp
)

target_compile_options(openmp_webserver PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(openmp_webserver PRIVATE pthread)
