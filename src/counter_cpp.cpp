// counter_cpp.cpp —— C/C++ 状态机
//
// 六个状态（Normal/String/Char/RawString/LineComment/BlockComment），逐字符
// 扫描；行归类规则与共享工具（isWs/isIdentChar/classifyLine）见
// counter_detail.h，I/O 层规范化见 counter.cpp。
//
// 对 .c 源文件的适用性：C 的全部词法构造（两种注释、字符串/字符字面量及
// 转义、行拼接 \、L"..." 宽字符串、预处理行）都在本状态机的规则集合内；
// 会触发 C++ 独有分支的序列——标识符 R 紧跟 "（原始字符串）、u8/u/U/L
// 编码前缀后紧跟 "——在合法 C 代码中不会出现，且此类序列即使出现在 C 的
// 字符串字面量内部，状态机也处于 StString 态而不检查 RawString 分支。
// 因此 RawString 等分支对 .c 文件永不误触发，行归类精确无误。
//
// 输入正文已由 countSource 完成规范化：只含 \n 换行、无 BOM。

#include "counter_detail.h"

#include <string>

namespace cloc {

namespace {

// 是否处于 C++11 原始字符串的起始（quotePos 指向 '"'）：
//   R"(...)" / u8R"(...)" / uR"(...)" / UR"(...)" / LR"(...)
// 词法采用 maximal munch：R 前若为标识符字符，则 R 属于更长的标识符
// （如 FOUR"x" 中的 R 属于 FOUR，后跟普通字符串），仅当整个前缀
// （含编码前缀 u8/u/U/L）之前也是边界时才构成原始字符串。
// 小写 r 不是 C++ 原始字符串前缀，不触发。
bool isRawStringStart(const std::string& t, size_t quotePos) {
    if (quotePos == 0 || t[quotePos - 1] != 'R') return false;
    const size_t r = quotePos - 1;                       // 'R' 的下标
    if (r == 0) return true;                             // 文件以 R" 开头
    const unsigned char prev = static_cast<unsigned char>(t[r - 1]);
    if (!isIdentChar(prev)) return true;                 // R 前是边界：空格、(、= 等
    if (prev == '8')                                     // u8R
        return r >= 2 && t[r - 2] == 'u' &&
               (r == 2 || !isIdentChar(static_cast<unsigned char>(t[r - 3])));
    if (prev == 'u' || prev == 'U' || prev == 'L')       // uR / UR / LR
        return r == 1 || !isIdentChar(static_cast<unsigned char>(t[r - 2]));
    return false;                                        // R 属于更长标识符
}

} // namespace

// =====================================================================
// C / C++ 状态机
// =====================================================================

enum CppState {
    StNormal,       // 普通代码
    StString,       // "..." 字符串字面量
    StChar,         // '...' 字符字面量
    StRawString,    // R"(...)" 原始字符串（C++11，含 u8R/uR/UR/LR 前缀；自定义定界符未支持）
    StLineComment,  // // 行注释
    StBlockComment  // /* ... */ 块注释
};

FileCount countCpp(const std::string& text) {
    FileCount fc;
    CppState st = StNormal;
    bool hasCode = false;   // 本行出现过代码字符（含字符串字面量内容）
    bool hasNonWs = false;  // 本行出现过任何非空白字符

    const size_t n = text.size();
    size_t i = 0;
    while (i < n) {
        const char ch = text[i];
        const unsigned char c = static_cast<unsigned char>(ch);

        if (ch == '\n') {
            // ---- 行结束：归类 ----
            ++fc.lines;
            classifyLine(hasCode, hasNonWs, fc);
            hasCode = false;
            hasNonWs = false;

            // ---- 行拼接：换行前最后一个字符是 '\' 时，注释/字符串延续 ----
            const bool continued = (i > 0 && text[i - 1] == '\\');

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
                if (isRawStringStart(text, i)) {
                    st = StRawString; hasCode = true; ++i;   // 只消费 '"'，'(' 在态内消费
                } else {
                    st = StString; hasCode = true; ++i;
                }
            } else if (ch == '\'') {
                st = StChar; hasCode = true; ++i;
            } else if (ch == '/' && i + 1 < n && text[i + 1] == '/') {
                st = StLineComment; hasNonWs = true; i += 2;
            } else if (ch == '/' && i + 1 < n && text[i + 1] == '*') {
                st = StBlockComment; hasNonWs = true; i += 2;
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
        classifyLine(hasCode, hasNonWs, fc);
    }

    return fc;
}

} // namespace cloc
