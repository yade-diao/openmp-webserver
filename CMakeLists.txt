cmake_minimum_required(VERSION 3.20)

project(openmp_webserver LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(openmp_webserver
    src/main.cpp
)

if(MSVC)
    # 让 Windows socket 相关 API（如 inet_ntop）可用的最低目标版本：Windows 10
    target_compile_definitions(openmp_webserver PRIVATE _WIN32_WINNT=0x0A00 NOMINMAX)
    target_compile_options(openmp_webserver PRIVATE /W4)
else()
    target_compile_options(openmp_webserver PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Windows 下必须链接 Winsock 库，否则 socket/bind/listen/accept/recv/send 等符号会链接失败
if(WIN32)
    target_link_libraries(openmp_webserver PRIVATE ws2_32)
endif()
