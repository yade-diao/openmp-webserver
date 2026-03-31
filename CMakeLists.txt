cmake_minimum_required(VERSION 3.20)

project(openmp_server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 03)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT UNIX)
    message(FATAL_ERROR "This implementation requires Linux/WSL (epoll).")
endif()

add_executable(openmp_webserver
    src/main.cpp
    src/server.cpp
    src/processor.cpp
)

add_executable(backend_compare
    src/backend_compare.cpp
)

find_package(OpenMP)

target_compile_options(openmp_webserver PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(openmp_webserver PRIVATE pthread)

target_compile_options(backend_compare PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(backend_compare PRIVATE pthread)

if(OpenMP_CXX_FOUND)
    target_link_libraries(openmp_webserver PRIVATE OpenMP::OpenMP_CXX)
    target_compile_definitions(openmp_webserver PRIVATE OPENMP_AVAILABLE=1)

    target_link_libraries(backend_compare PRIVATE OpenMP::OpenMP_CXX)
    target_compile_definitions(backend_compare PRIVATE OPENMP_AVAILABLE=1)
endif()
