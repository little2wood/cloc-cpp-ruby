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
    # MinGW：静态链接 libstdc++/libgcc/winpthread，避免同名运行时 DLL 的
    # 版本地狱（C:\MinGW、Git、Strawberry 各带一份 ABI 不同的
    # libstdc++-6.dll，动态链接时按 PATH 顺序可能加载到错误版本）
    target_link_options(cloc PRIVATE -static -static-libgcc -static-libstdc++)
endif()
