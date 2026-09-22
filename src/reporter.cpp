// reporter.cpp —— 统计表格输出
//
// 输出格式（按题目样例的文字规格实现，格式集中在本文件便于微调）：
//   * 数据名与数据在同一列内右对齐
//   * 相邻数据名尾字符相距 13（即每个数据场宽 13，首个场宽 6）
//   * Files 左侧恰好 1 个空格（宽度 6 的场右对齐 5 字符词自然产生），
//     Blanks 右侧恰好 1 个空格后到达分隔线末端 -> 分隔线共 59 个 '-'
//   * 表头：Files / Lines / Code / Comments / Blanks（无 Language 列）
//   * 恒远只输出一行总计数据（C/C++/Ruby 合并，不分语言、无 SUM 行）
//   * 恒等式 Lines = Code + Comments + Blanks 由 main 的断言保证

#include "reporter.h"

#include <cstdio>
#include <ostream>
#include <string>

namespace cloc {

namespace {

const std::string kRule(59, '-');

void writeHeader(std::ostream& os) {
    char buf[80];
    std::snprintf(buf, sizeof(buf), "%6s%13s%13s%13s%13s",
                  "Files", "Lines", "Code", "Comments", "Blanks");
    os << buf << "\n";
}

void writeRow(std::ostream& os, long files, long lines, long code,
              long comments, long blanks) {
    char buf[80];
    std::snprintf(buf, sizeof(buf), "%6ld%13ld%13ld%13ld%13ld",
                  files, lines, code, comments, blanks);
    os << buf << "\n";
}

} // namespace

void report(const Summary& s, std::ostream& os) {
    os << kRule << "\n";
    writeHeader(os);
    os << kRule << "\n";
    writeRow(os, s.total.files, s.total.lines, s.total.code,
             s.total.comments, s.total.blanks);
    os << kRule << "\n";
}

} // namespace cloc
