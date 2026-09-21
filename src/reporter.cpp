// reporter.cpp —— cloc 风格统计表格输出
//
// 格式集中在本文件，便于与题目样例输出逐字符对齐：
//   * 79 个 '-' 的分隔线
//   * 列序：Language / files / lines / code / comments / blanks
//   * 数字列右对齐（列宽 10/10/10/11/11），语言列左对齐（列宽 15）

#include "reporter.h"

#include <cstdio>
#include <ostream>
#include <string>

namespace cloc {

namespace {

const std::string kRule(79, '-');

void writeHeader(std::ostream& os) {
    char buf[128];
    std::snprintf(buf, sizeof(buf), "%-15s%10s%10s%10s%11s%11s",
                  "Language", "files", "lines", "code", "comments", "blanks");
    os << buf << "\n";
}

void writeRow(std::ostream& os, const char* lang, long files, long lines,
              long code, long comments, long blanks) {
    char buf[128];
    std::snprintf(buf, sizeof(buf), "%-15s%10ld%10ld%10ld%11ld%11ld",
                  lang, files, lines, code, comments, blanks);
    os << buf << "\n";
}

} // namespace

void report(const Summary& s, std::ostream& os) {
    long totalFiles = 0;
    for (size_t k = 0; k < s.languages.size(); ++k)
        totalFiles += s.languages[k].files;

    os << totalFiles << " text file" << (totalFiles == 1 ? "." : "s.") << "\n";
    os << totalFiles << " unique file" << (totalFiles == 1 ? "." : "s.") << "\n";
    os << s.ignoredFiles << " file" << (s.ignoredFiles == 1 ? "" : "s")
       << " ignored.\n";
    os << "\n";

    os << kRule << "\n";
    writeHeader(os);
    os << kRule << "\n";
    for (size_t k = 0; k < s.languages.size(); ++k) {
        const LanguageStats& l = s.languages[k];
        writeRow(os, l.language.c_str(), l.files, l.lines, l.code,
                 l.comments, l.blanks);
    }

    if (s.languages.size() > 1) {
        long f = 0, ln = 0, co = 0, cm = 0, bl = 0;
        for (size_t k = 0; k < s.languages.size(); ++k) {
            const LanguageStats& l = s.languages[k];
            f += l.files; ln += l.lines; co += l.code;
            cm += l.comments; bl += l.blanks;
        }
        os << kRule << "\n";
        writeRow(os, "SUM", f, ln, co, cm, bl);
    }
    os << kRule << "\n";

    if (s.errorFiles > 0)
        os << s.errorFiles << " file" << (s.errorFiles == 1 ? "" : "s")
           << " could not be read.\n";
}

} // namespace cloc
