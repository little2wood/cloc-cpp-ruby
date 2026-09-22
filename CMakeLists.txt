cmake_minimum_required(VERSION 3.15)
project(cloc CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(cloc
    src/main.cpp
    src/counter.cpp
    src/scanner.cpp
    src/reporter.cpp
)

# GCC 8 的 std::filesystem 需要显式链接（GCC 9+ / MSVC 不需要）
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.0)
    target_link_libraries(cloc PRIVATE stdc++fs)
endif()

if(MSVC)
    # 源码为 UTF-8 编码（含中文注释）
    target_compile_options(cloc PRIVATE /utf-8 /W4)
else()
    target_compile_options(cloc PRIVATE -Wall -Wextra)
endif()
