cmake_minimum_required(VERSION 3.20)

project(openmp_server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 98)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT UNIX)
    message(FATAL_ERROR "This implementation requires a UNIX-like platform with epoll support.")
endif()

add_executable(openmp_webserver
    src/main.cpp
    src/server.cpp
    src/processor.cpp
    src/backend_serial.cpp
    src/backend_pthread.cpp
    src/backend_openmp.cpp
)

find_package(OpenMP)

target_compile_options(openmp_webserver PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(openmp_webserver PRIVATE pthread)

if(OpenMP_CXX_FOUND)
    target_link_libraries(openmp_webserver PRIVATE OpenMP::OpenMP_CXX)
    target_compile_definitions(openmp_webserver PRIVATE OPENMP_AVAILABLE=1)
endif()
