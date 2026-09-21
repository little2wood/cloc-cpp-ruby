// counter.cpp —— 核心：字符级状态机，统计单个源文件
//
// 设计要点：
//  * 逐字符扫描（而非逐行字符串查找），保证字符串/字符字面量中的
//    "//"、"/*" 不会被误判为注释（题目点名的边缘状况）；
//  * 每个物理行维护 hasCode / hasNonWs 两个标志，行结束时归类：
//      hasCode       -> 代码行
//      否则 hasNonWs -> 注释行
//      否则          -> 空行（含块注释内部的空白行，题目明确要求）
//  * 行尾反斜杠（C++ 翻译阶段 1 的行拼接）使注释/字符串延续到下一物理行。

#include "counter.h"

#include <cctype>
#include <istream>
#include <iterator>
#include <string>

namespace cloc {

namespace {

enum State {
    StNormal,       // 普通代码
    StString,       // "..." 字符串字面量
    StChar,         // '...' 字符字面量
    StRawString,    // R"(...)" 原始字符串（C++11，基本支持）
    StLineComment,  // // 行注释
    StBlockComment  // /* ... */ 块注释
};

inline bool isWs(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\v' || c == '\f';
}

inline bool isIdentChar(unsigned char c) {
    return std::isalnum(c) != 0 || c == '_';
}

} // namespace

FileCount countSource(std::istream& in) {
    std::string text;
    text.assign(std::istreambuf_iterator<char>(in),
                std::istreambuf_iterator<char>());

    // 剥离 UTF-8 BOM
    if (text.size() >= 3 &&
        static_cast<unsigned char>(text[0]) == 0xEF &&
        static_cast<unsigned char>(text[1]) == 0xBB &&
        static_cast<unsigned char>(text[2]) == 0xBF) {
        text.erase(0, 3);
    }

    FileCount fc;
    State st = StNormal;
    bool hasCode = false;   // 本行出现过代码字符（含字符串字面量内容）
    bool hasNonWs = false;  // 本行出现过任何非空白字符

    const size_t n = text.size();
    size_t i = 0;
    while (i < n) {
        const char ch = text[i];
        const unsigned char c = static_cast<unsigned char>(ch);

        // \r\n / \r 统一按换行处理：先跳过 \r
        if (ch == '\r') { ++i; continue; }

        if (ch == '\n') {
            // ---- 行结束：归类 ----
            ++fc.lines;
            if (hasCode)       ++fc.code;
            else if (hasNonWs) ++fc.comments;
            else               ++fc.blanks;
            hasCode = false;
            hasNonWs = false;

            // ---- 行拼接：换行前最后一个字符是 '\' 时，注释/字符串延续 ----
            size_t j = i;
            while (j > 0 && text[j - 1] == '\r') --j;
            const bool continued = (j > 0 && text[j - 1] == '\\');

            if (st == StLineComment) {
                if (!continued) st = StNormal;
            } else if (st == StString || st == StChar) {
                // 未闭合字面量：换行视为终止，避免错误扩散到整个文件
                if (!continued) st = StNormal;
            }
            // StNormal / StBlockComment / StRawString 状态跨行保持
            ++i;
            continue;
        }

        switch (st) {
        case StNormal:
            if (ch == '"') {
                st = StString; hasCode = true; ++i;
            } else if (ch == '\'') {
                st = StChar; hasCode = true; ++i;
            } else if (ch == '/' && i + 1 < n && text[i + 1] == '/') {
                st = StLineComment; hasNonWs = true; i += 2;
            } else if (ch == '/' && i + 1 < n && text[i + 1] == '*') {
                st = StBlockComment; hasNonWs = true; i += 2;
            } else if ((ch == 'R' || ch == 'r') && i + 1 < n &&
                       text[i + 1] == '"' &&
                       (i == 0 ||
                        !isIdentChar(static_cast<unsigned char>(text[i - 1])))) {
                st = StRawString; hasCode = true; i += 2;
            } else {
                if (!isWs(c)) hasCode = true;
                ++i;
            }
            break;

        case StString:
        case StChar: {
            const char quote = (st == StString) ? '"' : '\'';
            if (ch == '\\' && i + 1 < n && text[i + 1] != '\n') {
                hasCode = true;   // 转义序列：一次消费两个字符
                i += 2;
            } else {
                if (ch == quote) st = StNormal;
                if (!isWs(c)) hasCode = true;
                ++i;
            }
            break;
        }

        case StRawString:
            if (ch == ')' && i + 1 < n && text[i + 1] == '"') {
                st = StNormal; hasCode = true; i += 2;
            } else {
                if (!isWs(c)) hasCode = true;
                ++i;
            }
            break;

        case StLineComment:
            if (!isWs(c)) hasNonWs = true;
            ++i;
            break;

        case StBlockComment:
            if (ch == '*' && i + 1 < n && text[i + 1] == '/') {
                st = StNormal; hasNonWs = true; i += 2;
            } else {
                if (!isWs(c)) hasNonWs = true;
                ++i;
            }
            break;
        }
    }

    // 文件不以换行结尾时补记最后一行
    if (hasCode || hasNonWs) {
        ++fc.lines;
        if (hasCode) ++fc.code;
        else         ++fc.comments;
    }

    return fc;
}

} // namespace cloc
