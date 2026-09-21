cmake_minimum_required(VERSION 3.15)
project(cloc CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(cloc
    src/main.cpp
    src/counter.cpp
    src/scanner.cpp
    src/reporter.cpp
)

if(MSVC)
    # 源码为 UTF-8 编码（含中文注释）
    target_compile_options(cloc PRIVATE /utf-8 /W4)
else()
    target_compile_options(cloc PRIVATE -Wall -Wextra)
endif()
