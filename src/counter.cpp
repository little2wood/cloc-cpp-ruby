// counter.cpp —— 核心：字符级状态机，统计单个源文件
//
// 通用设计（C++ 与 Ruby 共用）：
//  * 逐字符扫描（而非逐行字符串查找），保证字符串/正则等字面量中的
//    注释标记不会被误判为注释（题目点名的边缘状况）；
//  * 每个物理行维护 hasCode / hasNonWs 两个标志，行结束时归类：
//      hasCode       -> 代码行
//      否则 hasNonWs -> 注释行
//      否则          -> 空行（含块注释内部的空白行，题目明确要求）
//  * countCpp  / countRuby 分别是两套状态转移表，由 countSource 分派。

#include "counter.h"

#include <cctype>
#include <cstring>
#include <istream>
#include <iterator>
#include <string>

namespace cloc {

namespace {

inline bool isWs(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\v' || c == '\f';
}

inline bool isIdentChar(unsigned char c) {
    return std::isalnum(c) != 0 || c == '_';
}

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

// 读入全部内容并剥离 UTF-8 BOM
std::string readAllText(std::istream& in) {
    std::string text;
    text.assign(std::istreambuf_iterator<char>(in),
                std::istreambuf_iterator<char>());
    if (text.size() >= 3 &&
        static_cast<unsigned char>(text[0]) == 0xEF &&
        static_cast<unsigned char>(text[1]) == 0xBB &&
        static_cast<unsigned char>(text[2]) == 0xBF) {
        text.erase(0, 3);
    }
    return text;
}

// 行结束时的三类归类
inline void classifyLine(bool hasCode, bool hasNonWs, FileCount& fc) {
    if (hasCode)       ++fc.code;
    else if (hasNonWs) ++fc.comments;
    else               ++fc.blanks;
}

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

        // 行结束：\n、\r\n 以及老式 Mac 的孤立 \r 均计为一行
        if (ch == '\n' || ch == '\r') {
            // \r\n 视为一个换行：\r 与其后的 \n 一并消费
            if (ch == '\r' && i + 1 < n && text[i + 1] == '\n') ++i;

            // ---- 行结束：归类 ----
            ++fc.lines;
            classifyLine(hasCode, hasNonWs, fc);
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

// =====================================================================
// Ruby 状态机
// =====================================================================

enum RubyState {
    RNormal,    // 普通代码
    RStr,       // '...' / "..." / `...` 字符串（strQuote 区分）
    RRegex,     // /.../ 正则字面量
    RPct,       // %w[] %q() %Q{} %r<> %{...} 等 percent 字面量
    RHeredoc,   // <<EOF ... EOF heredoc（等待结束标识符行）
    RLineC,     // # ... 行注释
    RBlockC     // =begin ... =end 块注释（两者都必须顶格）
};

bool startsWithAt(const std::string& t, size_t i, const char* s) {
    for (size_t k = 0; s[k] != '\0'; ++k) {
        if (i + k >= t.size() || t[i + k] != s[k]) return false;
    }
    return true;
}

// pos 处必须是空白、行尾或 EOF（=begin/=end 标记词的边界）
bool followedByWsOrEol(const std::string& t, size_t pos) {
    if (pos >= t.size()) return true;
    const unsigned char c = static_cast<unsigned char>(t[pos]);
    return isWs(c) || c == '\r' || c == '\n';
}

// lastMeaningful 是"操作数结尾"字符（标识符、数字、右括号、闭合引号等）
// -> 其后的 / 与 % 与 << 是运算符（除法、取模、移位），否则是字面量起始。
bool isOperandPos(unsigned char lastMean) {
    if (lastMean == 0) return false;              // 行首
    return std::isalnum(lastMean) != 0 || lastMean == '_' ||
           lastMean == ')' || lastMean == ']' || lastMean == '}' ||
           lastMean == '"' || lastMean == '\'' || lastMean == '`';
}

// Ruby 关键字：其后出现的 /re/ 与 %w[] 等一定是字面量（关键字不是操作数）
bool isRubyKeyword(const std::string& w) {
    static const char* const kw[] = {
        "and", "begin", "break", "case", "defined", "do", "else",
        "elsif", "end", "if", "next", "not", "or", "redo", "rescue",
        "retry", "return", "super", "then", "unless", "until", "when",
        "while", "yield"
    };
    for (size_t k = 0; k < sizeof(kw) / sizeof(kw[0]); ++k)
        if (w == kw[k]) return true;
    return false;
}

// percent 字面量的定界符（保守集合：排除 = 与 # 及引号，避免与运算符/注释混淆）
bool isPctDelimiter(unsigned char c) {
    return c == '(' || c == '[' || c == '{' || c == '<' || c == '%' ||
           c == '!' || c == '@' || c == '^' || c == '~' || c == '$' ||
           c == '&' || c == '*' || c == '+' || c == '-' || c == '|' ||
           c == ',' || c == '.' || c == '?' || c == ';' || c == ':' ||
           c == '/' || c == '\\';
}

unsigned char matchingCloseOf(unsigned char open) {
    switch (open) {
        case '(': return ')';
        case '[': return ']';
        case '{': return '}';
        case '<': return '>';
        default:  return open;   // 同字符定界，如 %...% %!...!
    }
}

FileCount countRuby(const std::string& text) {
    FileCount fc;
    RubyState st = RNormal;

    char strQuote = 0;             // RStr 态当前引号：'"' '\'' '`'
    int  ivDepth = 0;              // "..." 中 #{...} 插值的嵌套深度
    unsigned char pctOpen = 0, pctClose = 0;
    int  pctNest = 0;
    std::string heredocId;         // heredoc 结束标识符
    bool heredocIndent = false;    // <<-EOF 允许结束行缩进
    bool heredocLineStart = false; // 当前处于行首匹配阶段
    size_t heredocMatchPos = 0;    // 已匹配的 id 前缀长度

    bool hasCode = false;
    bool hasNonWs = false;
    long col = 0;                  // 当前列（1 起），用于顶格判定
    unsigned char lastMean = 0;    // Normal 态上一个非空白字符
    std::string wordBuf;           // Normal 态正在累积的词
    std::string lastWord;          // 上一个已完结的词（关键字判定用）

    // 当前位置是否处于"操作数结尾"（决定 / % ? 是运算符还是字面量起始）
    // 关键字（when/if/return...）结尾不算操作数
    struct OperandEndCheck {
        unsigned char& lastMean;
        std::string& lastWord;
        bool operator()() const {
            if (!isOperandPos(lastMean)) return false;
            if (isIdentChar(lastMean) && isRubyKeyword(lastWord)) return false;
            return true;
        }
    } atOperandEnd = { lastMean, lastWord };

    const size_t n = text.size();
    size_t i = 0;
    while (i < n) {
        const char ch = text[i];
        const unsigned char c = static_cast<unsigned char>(ch);

        // 行结束：\n、\r\n 以及老式 Mac 的孤立 \r 均计为一行
        if (ch == '\n' || ch == '\r') {
            // \r\n 视为一个换行：\r 与其后的 \n 一并消费
            if (ch == '\r' && i + 1 < n && text[i + 1] == '\n') ++i;

            // heredoc 结束行判定：行首完整匹配到 id 且到达行尾
            if (st == RHeredoc && heredocLineStart &&
                heredocMatchPos == heredocId.size()) {
                st = RNormal;      // 结束行本身按代码行处理
            }
            ++fc.lines;
            classifyLine(hasCode, hasNonWs, fc);
            hasCode = false;
            hasNonWs = false;
            col = 0;
            lastMean = 0;
            wordBuf.clear();
            lastWord.clear();
            if (st == RLineC) st = RNormal;
            if (st == RRegex) st = RNormal;      // 正则字面量不允许跨行
            if (st == RHeredoc) {
                heredocLineStart = true;
                heredocMatchPos = 0;
            }
            ++i;
            continue;
        }
        ++col;

        switch (st) {
        case RNormal: {
            // 维护"上一个完整词"（用于关键字判定：when/if/return 等不是操作数）
            if (isIdentChar(c)) {
                wordBuf += ch;
            } else if (!wordBuf.empty()) {
                lastWord = wordBuf;
                wordBuf.clear();
            }

            if (isWs(c)) { ++i; break; }

            if (ch == '#') {                       // 行注释
                st = RLineC; hasNonWs = true; ++i; break;
            }
            if (ch == '\'' || ch == '"' || ch == '`') {
                st = RStr; strQuote = ch; ivDepth = 0;
                hasCode = true; lastMean = c; ++i; break;
            }
            // =begin 必须顶格，且后随空白/行尾（=beginning 等不算）
            if (ch == '=' && col == 1 &&
                startsWithAt(text, i, "=begin") &&
                followedByWsOrEol(text, i + 6)) {
                st = RBlockC; hasNonWs = true; i += 6; break;
            }
            // percent 字面量：%w[...] %q(...) %Q{...} %{...} 等
            // 处于操作数结尾（含关键字前缀例外）时是取模运算符（a % b）
            if (ch == '%' && !atOperandEnd()) {
                size_t j = i + 1;
                if (j < n && std::isalnum(static_cast<unsigned char>(text[j])) != 0)
                    ++j;                            // 可选类型字母 w q Q i I r s
                if (j < n && isPctDelimiter(static_cast<unsigned char>(text[j]))) {
                    pctOpen = static_cast<unsigned char>(text[j]);
                    pctClose = matchingCloseOf(pctOpen);
                    pctNest = 1;
                    st = RPct; hasCode = true;
                    lastMean = pctClose;
                    i = j + 1; break;
                }
                // lookahead 失败（如 %=、% 空格）：按普通字符处理
            }
            // 正则字面量：/ 处于操作数结尾是除法（a / b）；
            // 关键字之后（when /re/、return /re/ 等）是正则
            if (ch == '/' && !atOperandEnd()) {
                st = RRegex; hasCode = true; ++i; break;
            }
            // heredoc：<< 紧跟（无空格）大写/下划线开头的标识符或引号标识符
            // （Ruby 词法规定 <<EOF 紧贴即 heredoc；带空格的 arr << item 是移位）
            if (ch == '<' && i + 1 < n && text[i + 1] == '<') {
                size_t j = i + 2;
                bool ind = false;
                if (j < n && (text[j] == '-' || text[j] == '~')) {
                    ind = true; ++j;
                }
                bool quoted = false;
                char qc = 0;
                if (j < n && (text[j] == '"' || text[j] == '\'')) {
                    quoted = true; qc = text[j]; ++j;
                }
                bool ok = false;
                std::string id;
                if (j < n) {
                    const unsigned char c0 = static_cast<unsigned char>(text[j]);
                    const bool idStart = quoted || std::isupper(c0) != 0 || c0 == '_';
                    if (idStart) {
                        size_t k = j;
                        while (k < n && isIdentChar(static_cast<unsigned char>(text[k]))) {
                            id += text[k]; ++k;
                        }
                        if (!id.empty()) {
                            if (quoted) {
                                if (k < n && text[k] == qc) ++k;
                                else id.clear();       // 引号未闭合：不是 heredoc
                            }
                            if (!id.empty()) {
                                i = k;
                                ok = true;
                            }
                        }
                    }
                }
                if (ok) {
                    st = RHeredoc;
                    heredocId = id;
                    heredocIndent = ind;
                    heredocLineStart = false;
                    heredocMatchPos = 0;
                    hasCode = true;
                    lastMean = 0;
                    break;
                }
                // lookahead 失败（如 <<=、<<3、arr << item）：按普通 << 处理
            }
            // ?x 字符字面量（? 处于非操作数结尾，如 x = ?a）
            // 三元运算符的 ? 处于操作数结尾，走普通分支
            if (ch == '?' && !atOperandEnd() && i + 1 < n) {
                const unsigned char nc = static_cast<unsigned char>(text[i + 1]);
                if (!isWs(nc) && nc != '\n' && nc != '\r') {
                    hasCode = true;
                    if (nc == '\\') i += 3;        // ?\n ?\\ 等转义形式
                    else            i += 2;
                    lastMean = 'x';
                    break;
                }
            }
            // 普通代码字符
            hasCode = true;
            lastMean = c;
            ++i;
            break;
        }

        case RStr:
            if (ch == '\\' && i + 1 < n && text[i + 1] != '\n') {
                hasCode = true;                     // 转义：一次消费两字符
                i += 2; break;
            }
            if ((strQuote == '"' || strQuote == '`') && ch == '#' &&
                i + 1 < n && text[i + 1] == '{') {
                ++ivDepth;                          // #{...} 插值开始
                hasCode = true; i += 2; break;
            }
            if ((strQuote == '"' || strQuote == '`') && ch == '}' && ivDepth > 0) {
                --ivDepth;                          // 插值结束
                hasCode = true; ++i; break;
            }
            if (ch == strQuote && ivDepth == 0) {
                st = RNormal;                       // 字符串闭合
                lastMean = static_cast<unsigned char>(strQuote);
                hasCode = true; ++i; break;
            }
            if (!isWs(c)) hasCode = true;           // 跨行字符串中的空白行算空行
            ++i;
            break;

        case RRegex:
            if (ch == '\\' && i + 1 < n) {
                hasCode = true; i += 2; break;
            }
            if (ch == '/') {
                st = RNormal;
                lastMean = '/';
                hasCode = true; ++i;
                // 正则标志字符（/re/i 等）直接消费
                while (i < n) {
                    const unsigned char f = static_cast<unsigned char>(text[i]);
                    if (f != '\n' && f != '\r' && !isWs(f) &&
                        std::strchr("imxouesn", static_cast<char>(f)) != 0 &&
                        std::isalpha(f) != 0) {
                        hasCode = true; ++i;
                    } else {
                        break;
                    }
                }
                break;
            }
            hasCode = true;                         // 正则内容里的 # 不是注释
            ++i;
            break;

        case RPct:
            if (ch == '\\' && i + 1 < n) {
                hasCode = true; i += 2; break;
            }
            if (static_cast<unsigned char>(ch) == pctClose) {
                if (pctOpen == pctClose) {
                    // 同字符定界（%Q^...^ 等）：遇定界符即闭合，不嵌套
                    st = RNormal;
                    lastMean = pctClose;
                    hasCode = true; ++i; break;
                }
                --pctNest;
                if (pctNest == 0) {
                    st = RNormal;
                    lastMean = pctClose;
                }
            } else if (static_cast<unsigned char>(ch) == pctOpen) {
                ++pctNest;                          // 配对括号定界可嵌套
            }
            if (!isWs(c)) hasCode = true;           // 内容中的空白行算空行
            ++i;
            break;

        case RHeredoc:
            if (heredocLineStart) {
                // 阶段 1：跳过结束行缩进（仅 <<- 允许）
                if (heredocMatchPos == 0 && isWs(c)) {
                    if (heredocIndent) { ++i; break; }
                    heredocLineStart = false;       // 无缩进容忍，此行不是结束行
                    hasCode = true; ++i; break;
                }
                // 阶段 2：逐字符匹配结束标识符
                if (heredocMatchPos < heredocId.size()) {
                    if (ch == heredocId[heredocMatchPos]) {
                        ++heredocMatchPos;
                        hasCode = true;             // 结束行（EOF 行）非空白，算代码行
                        ++i; break;
                    }
                    heredocLineStart = false;       // 失配：普通内容行
                    hasCode = true; ++i; break;
                }
                // 阶段 3：id 已完整匹配，容忍行尾空白；其他字符则失配
                if (isWs(c)) { ++i; break; }
                heredocLineStart = false;
                hasCode = true; ++i; break;
            }
            if (!isWs(c)) hasCode = true;           // heredoc 内容中的空白行算空行
            ++i;
            break;

        case RLineC:
            if (!isWs(c)) hasNonWs = true;
            ++i;
            break;

        case RBlockC:
            // =end 必须顶格且是独立词
            if (ch == '=' && col == 1 &&
                startsWithAt(text, i, "=end") &&
                followedByWsOrEol(text, i + 4)) {
                st = RNormal; hasNonWs = true; i += 4; break;
            }
            if (!isWs(c)) hasNonWs = true;
            ++i;
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

} // namespace

FileCount countSource(std::istream& in, SourceLanguage lang) {
    const std::string text = readAllText(in);
    if (lang == LangRuby) return countRuby(text);
    return countCpp(text);
}

} // namespace cloc
