// scanner.cpp —— 目录递归遍历（Win32 / POSIX 双实现）与扩展名过滤
//
// Windows 用 FindFirstFileA/FindNextFileA（MSVC 与 MinGW 均可用）；
// Linux/macOS 用 dirent.h + stat。跳过符号链接/junction 防止循环遍历。

#include "scanner.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

namespace cloc {

namespace {

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

std::string joinPath(const std::string& dir, const std::string& name) {
    if (!dir.empty() && dir[dir.size() - 1] != '/' &&
        dir[dir.size() - 1] != '\\')
        return dir + "/" + name;
    return dir + name;
}

#ifdef _WIN32

void walkDirectory(const std::string& dir, std::vector<std::string>& out,
                   long& ignored) {
    WIN32_FIND_DATAA fd;
    const HANDLE h = FindFirstFileA(joinPath(dir, "*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        const std::string name(fd.cFileName);
        if (name == "." || name == "..") continue;
        const std::string full = joinPath(dir, name);
        if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            // 跳过符号链接 / junction，避免循环遍历
            if ((fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
                continue;
            walkDirectory(full, out, ignored);
        } else if (hasSourceExtension(name)) {
            out.push_back(full);
        } else {
            ++ignored;
        }
    } while (FindNextFileA(h, &fd) != 0);
    FindClose(h);
}

bool isDirectory(const std::string& path) {
    const DWORD attr = GetFileAttributesA(path.c_str());
    return attr != INVALID_FILE_ATTRIBUTES &&
           (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

#else // POSIX（Linux / macOS）

void walkDirectory(const std::string& dir, std::vector<std::string>& out,
                   long& ignored) {
    DIR* d = opendir(dir.c_str());
    if (d == 0) return;
    struct dirent* e;
    while ((e = readdir(d)) != 0) {
        const std::string name(e->d_name);
        if (name == "." || name == "..") continue;
        const std::string full = joinPath(dir, name);
        struct stat st;
        if (stat(full.c_str(), &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) {
            walkDirectory(full, out, ignored);
        } else if (S_ISREG(st.st_mode) && hasSourceExtension(name)) {
            out.push_back(full);
        } else {
            ++ignored;
        }
    }
    closedir(d);
}

bool isDirectory(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

#endif

} // namespace

ScanResult collectSources(const std::string& path) {
    ScanResult result;
    if (isDirectory(path)) {
        walkDirectory(path, result.sourceFiles, result.ignoredFiles);
    } else if (hasSourceExtension(path)) {
        result.sourceFiles.push_back(path);  // 允许直接传入单个源文件
    } else {
        result.ignoredFiles = 1;             // 传入的不是目录也不是源文件
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
