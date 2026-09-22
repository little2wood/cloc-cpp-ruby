// scanner.cpp —— 目录递归遍历与扩展名过滤（std::filesystem，C++17）
//
// 使用 recursive_directory_iterator 递归遍历：
//   * skip_permission_denied：无权限目录自动跳过
//   * 默认不跟随目录符号链接（等价于旧实现跳过 junction/reparse point，
//     避免循环遍历）
//   * 全程 error_code 重载，不抛异常
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
    const std::string::size_type dot = name.find_last_of('.');
    if (dot == std::string::npos) return "";
    std::string ext = name.substr(dot + 1);
    for (std::string::size_type k = 0; k < ext.size(); ++k)
        ext[k] = static_cast<char>(
            std::tolower(static_cast<unsigned char>(ext[k])));
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

    const fs::path root(path);

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
                const std::string full = it->path().string();
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

std::string languageOf(const std::string& path) {
    const std::string ext = extensionOf(path);
    if (ext == "c") return "C";
    if (ext == "cpp" || ext == "cc") return "C++";
    if (ext == "rb" || ext == "rbw") return "Ruby";
    return "";
}

} // namespace cloc
