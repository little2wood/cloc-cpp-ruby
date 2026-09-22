// counter_detail.h —— counter 模块内部共享定义
//
// 仅由 counter.cpp 与 counter_ruby.cpp 包含，不属于公共 API
// （对外入口只有 counter.h 的 countSource）。
//
// 两台状态机共享的内容：
//   * isWs / isIdentChar / classifyLine：字符分类与行结束时的三类归类
//     （题目规则：代码行 / 注释行 / 空行）；
//   * countCpp / countRuby：两套字符级状态转移的声明。输入正文均为已做
//     过 I/O 层规范化的文本（只含 \n 换行、无 BOM），分别定义在
//     counter_cpp.cpp 与 counter_ruby.cpp。
#pragma once

#include <cctype>
#include <string>

#include "counter.h"

namespace cloc {

inline bool isWs(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\v' || c == '\f';
}

inline bool isIdentChar(unsigned char c) {
    return std::isalnum(c) != 0 || c == '_';
}

// 行结束时的三类归类
inline void classifyLine(bool hasCode, bool hasNonWs, FileCount& fc) {
    if (hasCode)       ++fc.code;
    else if (hasNonWs) ++fc.comments;
    else               ++fc.blanks;
}

FileCount countCpp(const std::string& text);
FileCount countRuby(const std::string& text);

} // namespace cloc
