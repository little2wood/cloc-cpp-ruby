// main.cpp —— cloc 入口：命令行解析、调度、汇总与断言自检
//
// 用法: cloc <目录或源文件> [--lang=<语言>] [-o <输出文件>]
//   --lang=c++    只统计 C/C++ 源文件（.c/.cpp/.cc）
//   --lang=ruby   只统计 Ruby 源文件（.rb/.rbw）
//   --lang=all    两种语言都统计（默认）
//   -o <文件>     将结果写入文件而非标准输出
// 退出码: 0 正常；1 用法/路径/输出文件错误；2 行数恒等式校验失败

#include "counter.h"
#include "reporter.h"
#include "scanner.h"

#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

void printUsage(const char* prog) {
    std::cerr << "用法: " << prog
              << " <目录或源文件> [--lang=<语言>] [-o <输出文件>]\n"
              << "递归统计源码的代码行、注释行与空行（支持 C/C++ 与 Ruby）。\n"
              << "  --lang=c++    只统计 C/C++ 源文件（.c/.cpp/.cc）\n"
              << "  --lang=ruby   只统计 Ruby 源文件（.rb/.rbw）\n"
              << "  --lang=all    两种语言都统计（默认）\n"
              << "  -o <文件>     将结果写入文件而非标准输出\n";
}

// 题目要求的自检断言：Lines == Code + Comments + Blanks。
// 用显式校验而非 assert()，保证 Release（NDEBUG）下依然生效。
bool checkInvariant(const std::string& where, long lines, long code,
                    long comments, long blanks) {
    if (lines == code + comments + blanks) return true;
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
    std::string outputFile;
    std::string langFilter;   // 空 / "c++" / "ruby"
    for (int i = 2; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "-o" && i + 1 < argc) {
            outputFile = argv[++i];
        } else if (arg == "--lang=c++" || arg == "--lang=cpp") {
            langFilter = "c++";
        } else if (arg == "--lang=ruby" || arg == "--lang=rb") {
            langFilter = "ruby";
        } else if (arg == "--lang=all") {
            langFilter.clear();
        } else {
            printUsage(argv[0]);
            return 1;
        }
    }

    cloc::ScanResult scan = cloc::collectSources(target);
    if (scan.sourceFiles.empty()) {
        std::cerr << "在 \"" << target << "\" 中未找到受支持的源文件"
                     "（.c/.cpp/.cc/.rb/.rbw，请检查路径）。\n";
        return 1;
    }

    // --lang 过滤：剔除不属于目标语言的文件（计入 ignored）
    if (!langFilter.empty()) {
        std::vector<std::string> kept;
        kept.reserve(scan.sourceFiles.size());
        for (size_t k = 0; k < scan.sourceFiles.size(); ++k) {
            const std::string lang = cloc::languageOf(scan.sourceFiles[k]);
            const bool isRuby = (lang == "Ruby");
            const bool wanted = (langFilter == "ruby") ? isRuby : !isRuby;
            if (wanted) kept.push_back(scan.sourceFiles[k]);
            else        ++scan.ignoredFiles;
        }
        scan.sourceFiles.swap(kept);
        if (scan.sourceFiles.empty()) {
            std::cerr << "--lang=" << langFilter
                      << " 过滤后没有可统计的源文件。\n";
            return 1;
        }
    }

    cloc::LanguageStats total;   // 跨语言累加的总计（输出恒为单行）
    long errorFiles = 0;
    std::string firstErrorPath;
    bool consistent = true;

    for (size_t k = 0; k < scan.sourceFiles.size(); ++k) {
        const std::string& path = scan.sourceFiles[k];
        std::ifstream in(path.c_str(), std::ios::in | std::ios::binary);
        if (!in) {
            if (firstErrorPath.empty()) firstErrorPath = path;
            ++errorFiles;
            continue;
        }

        const cloc::SourceLanguage sl = (cloc::languageOf(path) == "Ruby")
                                            ? cloc::LangRuby
                                            : cloc::LangCpp;
        const cloc::FileCount fc = cloc::countSource(in, sl);

        // 断言 1：单文件行数自洽（题目要求）
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

    if (outputFile.empty()) {
        cloc::report(summary, std::cout);
    } else {
        std::ofstream out(outputFile.c_str());
        if (!out) {
            std::cerr << "无法打开输出文件: " << outputFile << "\n";
            return 1;
        }
        cloc::report(summary, out);
        std::cout << "统计结果已写入 " << outputFile << "\n";
    }

    // 诊断信息走 stderr，保证 stdout 的表格与题目样例逐字符一致
    if (errorFiles > 0)
        std::cerr << errorFiles << " 个文件无法读取，已跳过。\n";

    return consistent ? 0 : 2;
}
