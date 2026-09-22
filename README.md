# cloc —— C/C++ 与 Ruby 源码行数统计工具

递归统计目录下源码的物理行数，分为**代码行 / 注释行 / 空行** 三类，并保证恒等式
`Lines = Code + Comments + Blanks` 恒成立（程序内置自检断言，失败时以退出码 2 报告）。

支持的语言：
- **C / C++**：`.c` / `.cpp` / `.cc`
- **Ruby**：`.rb` / `.rbw`

## 功能特性

- 递归扫描目录（也可直接传入单个源文件）；非源码文件自动跳过并计入 ignored
- **字符级状态机**解析，正确处理：
  - 字符串 / 字符字面量中的 `//`、`/*`、`#`（**不**算注释）
  - `/* ... */` 跨行块注释；多行注释内部的空行计为**空行**
  - 转义字符：`"//"`、`'\''`、`'\\'` 等
  - 行尾 `\` 续行：注释 / 字符串延续到下一物理行
  - `//`、`///` 行注释；`\r\n` / `\r` 换行；UTF-8 BOM
  - C++11 原始字符串 `R"(...)"` 及全部编码前缀 `u8R`/`uR`/`UR`/`LR`（maximal munch 词法判定）
  - **Ruby**：`=begin`/`=end` 顶格块注释、`#` 行注释、正则字面量 `/.../`
    （含 `/ 除法` 歧义的关键字判定）、`%w[]`/`%Q{}` 等 percent 字面量（含嵌套
    定界与同字符定界）、`<<EOF`/`<<-EOF`/`<<~EOF` heredoc、`` `...` `` 反引号命令串、
    `"#{...}"` 插值、`?x` 字符字面量、shebang 行
- 恒定输出单行总计：即使目录中 C/C++ 与 Ruby 混合，也合并为一行汇总（无 SUM 行、无语言分类列）
- `--lang=c++` / `--lang=ruby` / `--lang=all` 语言过滤选项
- 结果输出到终端，或 `-o <文件>` 写入文件
- 跨平台：基于 C++17 `std::filesystem` 递归遍历，Windows / Linux / macOS 通用

## 编译建造指南

依赖：CMake ≥ 3.15 + 支持 C++17 的编译器（MSVC 2017 15.7+、GCC 9+、Clang 7+；GCC 8 由 CMake 自动附加 `stdc++fs` 链接）。

### 方式一：CLion（Windows）

1. 用 CLion 打开项目根目录（含 `CMakeLists.txt`），等待 CMake 自动加载；
2. 工具链选 MSVC 或 MinGW 均可，点击 **Build**（`Ctrl+F9`）；
3. 可执行文件生成在 `cmake-build-<配置>/cloc.exe`。

### 方式二：命令行（Windows，PowerShell）

```powershell
cmake -S . -B build
cmake --build build --config Release
.\build\Release\cloc.exe <目录>        # VS 生成器
.\build\cloc.exe <目录>                # Ninja / MinGW 生成器
```

### 方式三：Linux / macOS

```bash
cmake -S . -B build && cmake --build build
./build/cloc <目录>
```

## 使用方法

```
cloc <目录或源文件> [--lang=<语言>] [-o <输出文件>]

--lang=c++     只统计 C/C++ 源文件（.c/.cpp/.cc）
--lang=ruby    只统计 Ruby 源文件（.rb/.rbw）
--lang=all     两种语言都统计（默认）
```

示例：

```powershell
.\build\cloc.exe D:\code\cpp-project
.\build\cloc.exe tests\case1.c
.\build\cloc.exe D:\code\ruby-project --lang=ruby
.\build\cloc.exe . -o result.txt
```

示例输出（题目测试用例 `tests/case1.c`，与实际运行逐字符一致）：

```
-----------------------------------------------------------
 Files        Lines         Code     Comments       Blanks
-----------------------------------------------------------
     1           12            5            5            2
-----------------------------------------------------------
```

退出码：`0` 正常；`1` 用法 / 路径 / 输出文件错误；`2` 行数恒等式自检失败。

## 运行测试

```powershell
powershell -ExecutionPolicy Bypass -File tests\verify.ps1
```

脚本自动定位（必要时构建）`cloc.exe`，运行四组用例并逐项比对数字：

| 用例 | 输入 | 期望（files lines code comments blanks） |
|---|---|---|
| C 题目测试用例 | `tests/case1.c` | `1 12 5 5 2` |
| C 边缘情况 | `tests/edge_cases/`（字符串内注释符、转义、块注释内空行、行尾 `\` 续行） | `1 12 5 6 1` |
| Ruby 题目测试用例 | `tests/case1.rb`（`=begin/=end` 块注释、`#` 行注释、代码行尾注释） | `1 7 1 4 2` |
| C++11 原始字符串 | `tests/rawstrings.cpp`（`u8R`/`uR`/`UR`/`LR` 前缀、跨行原始串、行尾注释） | `1 11 10 1 0` |

## 项目结构

```
CMakeLists.txt          构建脚本（C++17，MSVC / MinGW-w64 / GCC 通用）
README.md               本文件：编译与使用指南
DESIGN.md               设计说明：状态机、归类规则、边缘情况清单
src/
  main.cpp              入口：参数解析（含 --lang）、调度、汇总与断言自检
  scanner.h / .cpp      目录递归遍历（C++17 std::filesystem）
  counter.h / .cpp      计数核心：I/O 规范化 + countSource 语言分派
  counter_cpp.cpp       C/C++ 状态机（六个状态）
  counter_detail.h      counter 模块内部共享定义（行归类规则与工具）
  counter_ruby.cpp      Ruby 状态机（加分项，独立文件）
  reporter.h / .cpp     cloc 风格表格输出（格式集中在该文件）
tests/
  case1.c               C 题目测试用例（12 行 = 5 代码 + 5 注释 + 2 空行）
  case1.rb              Ruby 题目测试用例（7 行 = 1 代码 + 4 注释 + 2 空行）
  rawstrings.cpp        C++11 原始字符串用例（u8R/uR/UR/LR，11 行 = 10 代码 + 1 注释）
  edge_cases/edge.cpp   边缘情况用例
  edge_cases/notes.txt  非源码文件（验证 ignored 计数）
  verify.ps1            自动对拍脚本
```

> 提交打包时仅包含上述源码与文档，**不要**包含 `build/`、`cmake-build-*/`
> 等构建产物或任何二进制文件。
