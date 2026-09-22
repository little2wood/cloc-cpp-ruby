// counter_ruby.cpp —— Ruby 状态机（加分项）
//
// 七个状态；核心难点是三类歧义的启发式判定（与 C++ 状态机共享行归类
// 规则与 isWs/isIdentChar/classifyLine，见 counter_detail.h）：
//  * / 除法 vs 正则、% 取模 vs percent 字面量、? 三元 vs 字符字面量：
//    维护"上一个非空白字符 + 上一个完整词"——处于操作数结尾且前一词
//    不是关键字时是运算符，否则是字面量起始；
//  * heredoc：<< 紧贴（无空格）大写/下划线/引号标识符即 heredoc，
//    带空格（arr << item）按移位处理；
//  * 插值："#{...}" 内维护嵌套深度，引号仅在深度 0 时闭合。
//
// 输入正文已由 countSource 完成规范化：只含 \n 换行、无 BOM。
// 状态与歧义规则的完整说明见 DESIGN.md 第 5 节。

#include "counter_detail.h"

#include <cstddef>
#include <cctype>
#include <cstring>
#include <string>

namespace cloc::detail {

namespace {

bool startsWithAt(const std::string& t, std::size_t i, const char* s) {
    for (std::size_t k = 0; s[k] != '\0'; ++k) {
        if (i + k >= t.size() || t[i + k] != s[k]) return false;
    }
    return true;
}

// pos 处必须是空白、行尾或 EOF（=begin/=end 标记词的边界）
bool followedByWsOrEol(const std::string& t, std::size_t pos) {
    if (pos >= t.size()) return true;
    const unsigned char c = static_cast<unsigned char>(t[pos]);
    return isWs(c) || c == '\n';
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
    for (std::size_t k = 0; k < sizeof(kw) / sizeof(kw[0]); ++k)
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

} // namespace

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
    std::size_t heredocMatchPos = 0;    // 已匹配的 id 前缀长度

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

    const std::size_t n = text.size();
    std::size_t i = 0;
    while (i < n) {
        const char ch = text[i];
        const unsigned char c = static_cast<unsigned char>(ch);

        if (ch == '\n') {
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
                std::size_t j = i + 1;
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
                std::size_t j = i + 2;
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
                        std::size_t k = j;
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
                if (!isWs(nc) && nc != '\n') {
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
                    if (f != '\n' && !isWs(f) &&
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

} // namespace cloc::detail
