# cloc —— C/C++ 源码行数统计工具

递归统计目录下 C/C++ 源码（`.c` / `.cpp` / `.cc`）的物理行数，分为
**代码行 / 注释行 / 空行** 三类，并保证恒等式 `Lines = Code + Comments + Blanks`
恒成立（程序内置自检断言，失败时以退出码 2 报告）。

## 功能特性

- 递归扫描目录（也可直接传入单个源文件）；非源码文件自动跳过并计入 ignored
- **字符级状态机**解析，正确处理：
  - 字符串 / 字符字面量中的 `//`、`/*`（**不**算注释）
  - `/* ... */` 跨行块注释；多行注释内部的空行计为**空行**
  - 转义字符：`"//"`、`'\''`、`'\\'` 等
  - 行尾 `\` 续行：注释 / 字符串延续到下一物理行
  - `//`、`///` 行注释；`\r\n` / `\r` 换行；UTF-8 BOM
  - C++11 原始字符串 `R"(...)"`（基本支持）
- 按语言（C / C++）分组汇总，多语言时输出 SUM 合计行
- 结果输出到终端，或 `-o <文件>` 写入文件
- 跨平台：Windows（FindFirstFile）与 Linux/macOS（dirent）双实现

## 编译建造指南

依赖：CMake ≥ 3.15 + 任意 C++11 编译器（MSVC、MinGW-w64 GCC、Clang、Linux GCC 均可）。

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
cloc <目录或源文件> [-o <输出文件>]
```

示例：

```powershell
.\build\cloc.exe D:\code\cpp-project
.\build\cloc.exe tests\case1.c
.\build\cloc.exe . -o result.txt
```

示例输出（题目测试用例 `tests/case1.c`，与实际运行逐字符一致）：

```
1 text file.
1 unique file.
0 files ignored.

-------------------------------------------------------------------------------
Language            files     lines      code   comments     blanks
-------------------------------------------------------------------------------
C                       1        12         5          5          2
-------------------------------------------------------------------------------
```

退出码：`0` 正常；`1` 用法 / 路径 / 输出文件错误；`2` 行数恒等式自检失败。

## 运行测试

```powershell
powershell -ExecutionPolicy Bypass -File tests\verify.ps1
```

脚本自动定位（必要时构建）`cloc.exe`，运行两组用例并逐项比对数字：

| 用例 | 输入 | 期望（files lines code comments blanks） |
|---|---|---|
| 题目测试用例 | `tests/case1.c` | `1 12 5 5 2` |
| 边缘情况 | `tests/edge_cases/`（字符串内注释符、转义、块注释内空行、行尾 `\` 续行） | `1 12 5 6 1` |

## 项目结构

```
CMakeLists.txt          构建脚本（C++11，MSVC / MinGW / GCC 通用）
README.md               本文件：编译与使用指南
DESIGN.md               设计说明：状态机、归类规则、边缘情况清单
src/
  main.cpp              入口：参数解析、调度、汇总与断言自检
  scanner.h / .cpp      目录递归遍历（Win32 / POSIX 双实现）
  counter.h / .cpp      核心：字符级状态机，统计单个文件
  reporter.h / .cpp     cloc 风格表格输出（格式集中在该文件）
tests/
  case1.c               题目测试用例（12 行 = 5 代码 + 5 注释 + 2 空行）
  edge_cases/edge.cpp   边缘情况用例
  edge_cases/notes.txt  非源码文件（验证 ignored 计数）
  verify.ps1            自动对拍脚本
```

> 提交打包时仅包含上述源码与文档，**不要**包含 `build/`、`cmake-build-*/`
> 等构建产物或任何二进制文件。
