// main.cpp —— cloc 入口：命令行解析、调度、汇总与断言自检
//
// 用法: cloc <目录或源文件> [--lang=<语言>]
//   --lang=c++    只统计 C/C++ 源文件（.c/.cpp/.cc）
//   --lang=ruby   只统计 Ruby 源文件（.rb/.rbw）
//   --lang=all    两种语言都统计（默认）
// 退出码: 0 正常；1 用法/路径错误；2 行数恒等式校验失败

#include "counter.h"
#include "reporter.h"
#include "scanner.h"

#include <cstddef>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {

#ifdef _WIN32
// 控制台 UTF-8 守卫（惰性）：诊断信息为 UTF-8 字节，Windows 控制台默认按
// GBK 解码会乱码。但 SetConsoleOutputCP 会触发 conhost 整屏重绘（且可能
// 切换字体），若在启动时切换，大目录统计的数秒运行期间画面会保持"拉伸"
// 状态。因此不在启动时切换，而是首次输出诊断信息前才切换；成功路径只
// 输出 ASCII 表格，全程不切换、无任何视觉扰动。进程退出时恢复原代码页
// （全局对象的析构保证覆盖所有返回路径）。
class ConsoleUtf8 {
public:
    // 注意：deleted 拷贝构造属于"用户声明的构造函数"，会抑制隐式默认
    // 构造，必须显式 default，否则全局对象 g_consoleUtf8 无法定义。
    ConsoleUtf8() = default;
    ~ConsoleUtf8() {
        if (switched_) SetConsoleOutputCP(old_);
    }
    void ensure() {
        if (!tried_) {
            tried_ = true;
            old_ = GetConsoleOutputCP();
            if (old_ != CP_UTF8) {   // 用户已手动 chcp 65001 时不再切换
                SetConsoleOutputCP(CP_UTF8);
                switched_ = true;
            }
        }
    }
    ConsoleUtf8(const ConsoleUtf8&) = delete;
    ConsoleUtf8& operator=(const ConsoleUtf8&) = delete;

private:
    bool tried_ = false;
    bool switched_ = false;
    UINT old_ = 0;
};

ConsoleUtf8 g_consoleUtf8;
inline void ensureConsoleUtf8() { g_consoleUtf8.ensure(); }
#else
inline void ensureConsoleUtf8() {}   // 非 Windows：窄字符即原生编码，无需处理
#endif

void printUsage(const char* prog) {
    ensureConsoleUtf8();
    std::cerr << "用法: " << prog
              << " <目录或源文件> [--lang=<语言>]\n"
              << "递归统计源码的代码行、注释行与空行（支持 C/C++ 与 Ruby）。\n"
              << "  --lang=c++    只统计 C/C++ 源文件（.c/.cpp/.cc）\n"
              << "  --lang=ruby   只统计 Ruby 源文件（.rb/.rbw）\n"
              << "  --lang=all    两种语言都统计（默认）\n";
}

// 题目要求的自检断言：Lines == Code + Comments + Blanks。
// 用显式校验而非 assert()，保证 Release（NDEBUG）下依然生效。
bool checkInvariant(const std::string& where, long lines, long code,
                    long comments, long blanks) {
    if (lines == code + comments + blanks) return true;
    ensureConsoleUtf8();
    std::cerr << "内部错误: " << where << " 行数不自洽: Lines(" << lines
              << ") != Code(" << code << ") + Comments(" << comments
              << ") + Blanks(" << blanks << ")\n";
    return false;
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    const std::string target = argv[1];
    bool haveLangFilter = false;   // --lang=all 时为 false（不过滤）
    cloc::SourceLanguage langFilter = cloc::LangCpp;
    std::string langName;          // 仅用于错误信息显示（"c++"/"ruby"）
    for (int i = 2; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--lang=c++" || arg == "--lang=cpp") {
            haveLangFilter = true;
            langFilter = cloc::LangCpp;
            langName = "c++";
        } else if (arg == "--lang=ruby" || arg == "--lang=rb") {
            haveLangFilter = true;
            langFilter = cloc::LangRuby;
            langName = "ruby";
        } else if (arg == "--lang=all") {
            haveLangFilter = false;
        } else {
            printUsage(argv[0]);
            return 1;
        }
    }

    cloc::ScanResult scan = cloc::collectSources(target);
    if (scan.sourceFiles.empty()) {
        ensureConsoleUtf8();
        std::cerr << "在 \"" << target << "\" 中未找到受支持的源文件"
                     "（.c/.cpp/.cc/.rb/.rbw，请检查路径）。\n";
        return 1;
    }

    // --lang 过滤：剔除不属于目标语言的文件（计入 ignored）。
    // 语言在参数解析阶段已映射为枚举，此处直接比较枚举，全链路类型安全。
    if (haveLangFilter) {
        std::vector<std::string> kept;
        kept.reserve(scan.sourceFiles.size());
        for (std::size_t k = 0; k < scan.sourceFiles.size(); ++k) {
            if (cloc::languageOf(scan.sourceFiles[k]) == langFilter)
                kept.push_back(scan.sourceFiles[k]);
            else
                ++scan.ignoredFiles;
        }
        scan.sourceFiles.swap(kept);
        if (scan.sourceFiles.empty()) {
            ensureConsoleUtf8();
            std::cerr << "--lang=" << langName
                      << " 过滤后没有可统计的源文件。\n";
            return 1;
        }
    }

    cloc::LanguageStats total;   // 跨语言累加的总计（输出恒为单行）
    long errorFiles = 0;
    std::string firstErrorPath;
    bool consistent = true;

    for (std::size_t k = 0; k < scan.sourceFiles.size(); ++k) {
        const std::string& path = scan.sourceFiles[k];
        std::ifstream in(path.c_str(), std::ios::in | std::ios::binary);
        if (!in) {
            if (firstErrorPath.empty()) firstErrorPath = path;
            ++errorFiles;
            continue;
        }

        const cloc::SourceLanguage sl = cloc::languageOf(path);
        const cloc::FileCount fc = cloc::countSource(in, sl);

        // 断言 1：单文件行数自洽（题目提及）
        if (!checkInvariant(path, fc.lines, fc.code, fc.comments, fc.blanks))
            consistent = false;

        ++total.files;
        total.lines += fc.lines;
        total.code += fc.code;
        total.comments += fc.comments;
        total.blanks += fc.blanks;
    }

    // 没有任何文件被成功读取：明确报错，而不是输出一张空表
    if (total.files == 0) {
        ensureConsoleUtf8();
        std::cerr << "无法打开 \"" << firstErrorPath
                  << "\"（文件不存在或不可读）。\n";
        return 1;
    }

    // 断言 2：总计行数自洽（各文件恒等式相加的复核）
    if (!checkInvariant("总计", total.lines, total.code,
                        total.comments, total.blanks))
        consistent = false;

    cloc::Summary summary;
    summary.total = total;
    summary.ignoredFiles = scan.ignoredFiles;
    summary.errorFiles = errorFiles;

    cloc::report(summary, std::cout);

    // 诊断信息走 stderr，保证 stdout 的表格与题目样例逐字符一致
    if (errorFiles > 0) {
        ensureConsoleUtf8();
        std::cerr << errorFiles << " 个文件无法读取，已跳过。\n";
    }

    return consistent ? 0 : 2;
}
