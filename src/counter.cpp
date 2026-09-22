// counter.cpp —— 计数核心枢纽：I/O 层规范化 + countSource 语言分派
//
// 通用设计（C++ 状态机见 counter_cpp.cpp，Ruby 见 counter_ruby.cpp）：
//  * 逐字符扫描（而非逐行字符串查找），保证字符串/正则等字面量中的
//    注释标记不会被误判为注释（题目点名的边缘状况）；
//  * 每个物理行维护 hasCode / hasNonWs 两个标志，行结束时按 classifyLine
//    归类（见 counter_detail.h）：
//      hasCode       -> 代码行
//      否则 hasNonWs -> 注释行
//      否则          -> 空行（含块注释内部的空白行，题目明确要求）
//  * I/O 层规范化（剥离 UTF-8 BOM、换行归一）在 readAllText 完成一次，
//    两台状态机此后只处理 \n。

#include "counter.h"
#include "counter_detail.h"

#include <istream>
#include <iterator>
#include <string>

namespace cloc {

namespace {

// 换行归一：\r\n 与老式 Mac 的孤立 \r 统一为 \n（原地双指针，单次 O(n)）。
// 归一属于 I/O 层规范化（与 BOM 剥离同级），两台状态机此后只处理 \n。
void normalizeNewlines(std::string& t) {
    size_t w = 0;
    for (size_t r = 0; r < t.size(); ++r) {
        const char c = t[r];
        if (c == '\r') {
            if (r + 1 < t.size() && t[r + 1] == '\n') continue;  // 交给 '\n'
            t[w++] = '\n';                                       // 孤立 \r
        } else {
            t[w++] = c;
        }
    }
    t.resize(w);
}

// 读入全部内容并做 I/O 层规范化：剥离 UTF-8 BOM、换行归一（\r\n 与孤立 \r
// 统一为 \n）
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
    normalizeNewlines(text);
    return text;
}

} // namespace

FileCount countSource(std::istream& in, SourceLanguage lang) {
    const std::string text = readAllText(in);
    if (lang == LangRuby) return countRuby(text);
    return countCpp(text);
}

} // namespace cloc
