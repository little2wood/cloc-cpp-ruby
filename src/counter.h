// counter.h —— 单文件行数统计接口
#pragma once

#include <iosfwd>

namespace cloc {

struct FileCount {
    long lines = 0;     // 物理总行数
    long code = 0;      // 代码行数
    long comments = 0;  // 注释行数
    long blanks = 0;    // 空行数
};

// 对输入流做字符级状态机扫描，返回行数统计。
// 要求以二进制方式打开（保留原始换行符）。
FileCount countSource(std::istream& in);

} // namespace cloc
