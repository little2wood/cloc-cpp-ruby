// scanner.h —— 目录遍历与源文件收集接口
#pragma once

#include <string>
#include <vector>

#include "counter.h"   // SourceLanguage（语言是 scanner/counter 共用的领域枚举）

namespace cloc {

struct ScanResult {
    std::vector<std::string> sourceFiles;  // 待统计的源码文件（完整路径，已排序）
    long ignoredFiles = 0;                 // 目录中被跳过的非源码文件数
};

// 递归收集 path（目录或单个文件）下所有受支持的源文件：
// C/C++：.c .cpp .cc（仅识别全小写，.C/.CPP/.Cc 等忽略）；
// Ruby：.rb/.rbw（不区分大小写）。其余文件一律跳过（计入 ignoredFiles）。
ScanResult collectSources(const std::string& path);

// 按扩展名返回语言枚举：.c/.cpp/.cc -> LangCpp；.rb/.rbw -> LangRuby。
// 前置条件：path 已通过 collectSources 的扩展名过滤（其余扩展名一律归入
// LangCpp，调用方不应依赖该行为）。枚举不区分 C 与 C++——两者共用同一
// 状态机，且本项目无按语言分列的输出需求。
SourceLanguage languageOf(const std::string& path);

} // namespace cloc
