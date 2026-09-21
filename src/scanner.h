// scanner.h —— 目录遍历与源文件收集接口
#pragma once

#include <string>
#include <vector>

namespace cloc {

struct ScanResult {
    std::vector<std::string> sourceFiles;  // 待统计的源码文件（完整路径，已排序）
    long ignoredFiles = 0;                 // 目录中被跳过的非源码文件数
};

// 递归收集 path（目录或单个文件）下所有受支持的源文件：
// C/C++：.c .cpp .cc；Ruby：.rb .rbw。其余文件一律跳过（计入 ignoredFiles）。
ScanResult collectSources(const std::string& path);

// 按扩展名返回语言名：.c -> "C"；.cpp/.cc -> "C++"；.rb/.rbw -> "Ruby"。
std::string languageOf(const std::string& path);

} // namespace cloc
