// scanner.cpp —— 目录递归遍历与扩展名过滤（std::filesystem，C++17）
//
// 使用 recursive_directory_iterator 递归遍历：
//   * skip_permission_denied：无权限目录自动跳过
//   * 默认不跟随目录符号链接（避免循环遍历）
//   * 遍历与状态查询使用 error_code 重载，不抛异常；会抛异常的只有两处
//     窄/宽字符串转换，均已处理：
//     - collectSources 入口的窄字符串 -> 宽路径（见下方 try/catch）；
//     - 枚举得到的宽路径 -> 窄字符串（见循环内 round-trip 校验）。
//
// 非 ASCII 文件名的行为依赖标准库实现（详见 DESIGN.md 第 8 节）：
//   * MSVC：string() 转换为 ANSI 代码页，无法表示的字符抛 filesystem_error
//     或被静默替换为 '?'（best-fit）——经 round-trip 校验捕获，按 ignored
//     跳过；
//   * libstdc++：string() 转换为 UTF-8（无损，round-trip 必过），文件可
//     进入统计，但后续窄字符 ifstream 按 ANSI 代码页解释文件名而打开
//     失败，由 main 计入 errorFiles（"无法读取"）。
//
// 业务规则（与遍历机制无关，保留）：
//   * 只收集 .c/.cpp/.cc/.rb/.rbw；其余普通文件计入 ignoredFiles
//   * 扩展名大小写归一（.CPP 当 .cpp）

#include "scanner.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>
#include <vector>

namespace cloc {

namespace {

namespace fs = std::filesystem;

std::string extensionOf(const std::string& name) {
    // 用 fs::path::extension() 提取扩展名：只作用于文件名部分（最后一个点
    // 必须在文件名内），目录名中的点不影响——"dir.cpp/README" 的扩展名是
    // 空而不是 "cpp/README"（对全路径做 find_last_of('.') 的旧实现依赖
    // "伪扩展名必含路径分隔符、无法匹配"这一巧合才不出错）。
    std::string ext = fs::path(name).extension().string();
    if (!ext.empty() && ext[0] == '.') ext.erase(0, 1);
    for (char& ch : ext)
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    return ext;
}

bool hasSourceExtension(const std::string& name) {
    const std::string ext = extensionOf(name);
    return ext == "c" || ext == "cpp" || ext == "cc" ||
           ext == "rb" || ext == "rbw";
}

} // namespace

ScanResult collectSources(const std::string& path) {
    ScanResult result;
    namespace fs = std::filesystem;
    std::error_code ec;

    // 窄字符串 -> 宽路径的转换可能抛异常：libstdc++（GCC 13 实测）按 UTF-8
    // 解释窄串，ANSI 代码页（如 GBK）的中文字节序列非法即抛 filesystem_error；
    // MSVC 按 ANSI 代码页转换，argv 中的字节必可表示，不会抛。捕获后返回
    // 空结果，由 main 报"未找到受支持的源文件"，进程优雅退出而非崩溃。
    fs::path root;
    try {
        root = fs::path(path);
    } catch (const std::filesystem::filesystem_error&) {
        return result;
    }

    if (fs::is_regular_file(root, ec)) {
        // 允许直接传入单个源文件
        if (hasSourceExtension(path)) result.sourceFiles.push_back(path);
        else ++result.ignoredFiles;
    } else {
        fs::recursive_directory_iterator it(
            root, fs::directory_options::skip_permission_denied, ec);
        fs::recursive_directory_iterator end;
        while (!ec && it != end) {
            std::error_code entryEc;
            if (it->is_regular_file(entryEc) && !entryEc) {
                // 宽路径 -> 窄字符串的行为随标准库实现而异（详见 DESIGN.md
                // 第 8 节）：MSVC 转换为 ANSI 代码页（中文系统即 GBK），文件
                // 名含 ACP 无法表示的字符（emoji、其他语言等）时有两种失败
                // 形态——转换失败抛 filesystem_error（标准允许），或转换
                // "成功"但不可表示字符被静默替换为 '?'（窄字符 API 的
                // best-fit 行为）导致文件名损坏；两者均由 try/catch +
                // round-trip 校验（fs::path(full) != it->path()）捕获，按
                // ignored 跳过，保证统计不中断。libstdc++ 转换为 UTF-8
                // （无损），此处的校验恒过，此类文件留给后续打开阶段处理
                // （失败计入 errorFiles）。
                std::string full;
                bool nameOk = false;
                try {
                    full = it->path().string();
                    nameOk = (fs::path(full) == it->path());
                } catch (const std::filesystem::filesystem_error&) {
                    nameOk = false;
                }
                if (!nameOk) {
                    ++result.ignoredFiles;
                    it.increment(ec);
                    continue;
                }
                if (hasSourceExtension(full)) result.sourceFiles.push_back(full);
                else ++result.ignoredFiles;   // 非源码文件计入 ignored
            }
            // 目录不计入 ignored（与旧实现一致）；无权限等错误经 ec 静默跳过
            it.increment(ec);
        }
    }

    std::sort(result.sourceFiles.begin(), result.sourceFiles.end());
    return result;
}

SourceLanguage languageOf(const std::string& path) {
    const std::string ext = extensionOf(path);
    if (ext == "rb" || ext == "rbw") return LangRuby;
    return LangCpp;   // .c/.cpp/.cc（collectSources 已过滤其余扩展名）
}

} // namespace cloc
