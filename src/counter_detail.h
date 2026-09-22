// counter_detail.h —— counter 模块内部共享定义
//
// 仅由 counter.cpp、counter_cpp.cpp 与 counter_ruby.cpp 包含，不属于公共 API
// （对外入口只有 counter.h 的 countSource）。内容位于 cloc::detail 命名
// 空间——"内部共享"由命名空间本身声明，与 cloc 公共命名空间明确区分。
//
// 两台状态机共享的内容：
//   * isWs / isIdentChar / classifyLine：字符分类与行结束时的三类归类
//     （题目规则：代码行 / 注释行 / 空行）；
//   * countCpp / countRuby：两套字符级状态转移的声明，分别定义在
//     counter_cpp.cpp 与 counter_ruby.cpp。
//
// 输入契约（两台状态机的共同前提）：传入的 text 已由 countSource 完成
// I/O 层规范化——UTF-8 BOM 已剥离，\r\n 与孤立 \r 均已归一为 \n，
// 因此正文只含 \n 一种换行、不存在 \r（状态机内不做 \r 判断）。
#pragma once

#include <cctype>
#include <string>

#include "counter.h"

namespace cloc::detail {

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

} // namespace cloc::detail
