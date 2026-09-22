// reporter.cpp —— 统计表格输出
//
// 输出格式（按题目样例的文字规格实现，格式集中在本文件便于微调）：
//   * 数据名与数据在同一列内右对齐
//   * 相邻数据名尾字符相距 13（即每个数据场宽 13，首个场宽 6）
//   * Files 左侧恰好 1 个空格（宽度 6 的场右对齐 5 字符词自然产生），
//     Blanks 右侧恰好 1 个空格后到达分隔线末端 -> 分隔线共 59 个 '-'
//   * 表头：Files / Lines / Code / Comments / Blanks（无 Language 列）
//   * 多语言时逐语言一行并附 SUM 合计行（题干保证目录不混合，正常只有一行）
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

void writeSumRow(std::ostream& os, long files, long lines, long code,
                 long comments, long blanks) {
    char buf[80];
    std::snprintf(buf, sizeof(buf), "%6s%13ld%13ld%13ld%13ld",
                  "SUM", files, lines, code, comments, blanks);
    os << buf << "\n";
}

} // namespace

void report(const Summary& s, std::ostream& os) {
    os << kRule << "\n";
    writeHeader(os);
    os << kRule << "\n";
    for (size_t k = 0; k < s.languages.size(); ++k) {
        const LanguageStats& l = s.languages[k];
        writeRow(os, l.files, l.lines, l.code, l.comments, l.blanks);
    }

    if (s.languages.size() > 1) {          // 题干保证不混合，此分支仅为健壮性
        long f = 0, ln = 0, co = 0, cm = 0, bl = 0;
        for (size_t k = 0; k < s.languages.size(); ++k) {
            const LanguageStats& l = s.languages[k];
            f += l.files; ln += l.lines; co += l.code;
            cm += l.comments; bl += l.blanks;
        }
        os << kRule << "\n";
        writeSumRow(os, f, ln, co, cm, bl);
    }
    os << kRule << "\n";
}

} // namespace cloc
