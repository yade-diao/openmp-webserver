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
    src/http_parser.cpp
    src/user_repository.cpp
    src/web_app.cpp
    src/logger.cpp
    src/data_processor.cpp
)

add_executable(legacy_backend_compare
    src/legacy_backend_compare.cpp
)

find_package(OpenMP)

target_compile_options(openmp_webserver PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(openmp_webserver PRIVATE pthread sqlite3)

target_compile_options(legacy_backend_compare PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(legacy_backend_compare PRIVATE pthread)

if(OpenMP_CXX_FOUND)
    target_link_libraries(openmp_webserver PRIVATE OpenMP::OpenMP_CXX)
    target_compile_definitions(openmp_webserver PRIVATE OPENMP_AVAILABLE=1)

    target_link_libraries(legacy_backend_compare PRIVATE OpenMP::OpenMP_CXX)
    target_compile_definitions(legacy_backend_compare PRIVATE OPENMP_AVAILABLE=1)
endif()
