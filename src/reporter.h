// reporter.h —— cloc 风格统计表格输出接口
#pragma once

#include <iosfwd>
#include <string>
#include <vector>

namespace cloc {

struct LanguageStats {
    std::string language;
    long files = 0;
    long lines = 0;
    long code = 0;
    long comments = 0;
    long blanks = 0;
};

struct Summary {
    std::vector<LanguageStats> languages;  // 按语言名排序（"C" 在 "C++" 之前）
    long ignoredFiles = 0;                 // 被跳过的非源码文件数
    long errorFiles = 0;                   // 打开/读取失败的文件数
};

// 将统计结果以 cloc 风格表格写入输出流。
void report(const Summary& s, std::ostream& os);

} // namespace cloc
