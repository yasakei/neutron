/*
 * Neutron Programming Language
 * Copyright (c) 2026 yasakei
 * 
 * This software is distributed under the Neutron Permissive License (NPL) 1.1.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, for both open source and commercial purposes.
 * 
 * Conditions:
 * 
 * 1. The above copyright notice and this permission notice shall be included
 *    in all copies or substantial portions of the Software.
 * 
 * 2. Attribution is appreciated but NOT required.
 *    Suggested (optional) credit:
 *    "Built using Neutron Programming Language (c) yasakei"
 * 
 * 3. The name "Neutron" and the name of the copyright holder may not be used
 *    to endorse or promote products derived from this Software without prior
 *    written permission.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "platform/platform.h"
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>

#ifdef _WIN32
    #include <windows.h>
    #include <direct.h>
    #include <lmcons.h>
    #define getcwd _getcwd
    #define chdir _chdir
    #define PATH_SEP "\\"
#else
    #include <unistd.h>
    #include <sys/stat.h>
    #include <sys/types.h>
    #include <dirent.h>
    #include <pwd.h>
    #ifdef __APPLE__
    #include <mach-o/dyld.h>
    #endif
    #define PATH_SEP "/"
#endif

namespace neutron {
namespace platform {

std::string getCwd() {
    char buffer[4096];
    if (getcwd(buffer, sizeof(buffer)) != nullptr) {
        return std::string(buffer);
    }
    return "";
}

bool setCwd(const std::string& path) {
#ifdef _WIN32
    return _chdir(path.c_str()) == 0;
#else
    return chdir(path.c_str()) == 0;
#endif
}

bool fileExists(const std::string& path) {
#ifdef _WIN32
    DWORD attrs = GetFileAttributesA(path.c_str());
    return (attrs != INVALID_FILE_ATTRIBUTES);
#else
    struct stat buffer;
    return (stat(path.c_str(), &buffer) == 0);
#endif
}

bool isDirectory(const std::string& path) {
#ifdef _WIN32
    DWORD attrs = GetFileAttributesA(path.c_str());
    return (attrs != INVALID_FILE_ATTRIBUTES) && 
           (attrs & FILE_ATTRIBUTE_DIRECTORY);
#else
    struct stat buffer;
    if (stat(path.c_str(), &buffer) != 0) {
        return false;
    }
    return S_ISDIR(buffer.st_mode);
#endif
}

std::vector<std::string> listDirectory(const std::string& path) {
    std::vector<std::string> result;
    
#ifdef _WIN32
    WIN32_FIND_DATAA findData;
    std::string searchPath = path + "\\*";
    HANDLE hFind = FindFirstFileA(searchPath.c_str(), &findData);
    
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            std::string name = findData.cFileName;
            if (name != "." && name != "..") {
                result.push_back(name);
            }
        } while (FindNextFileA(hFind, &findData));
        FindClose(hFind);
    }
#else
    DIR* dir = opendir(path.c_str());
    if (dir != nullptr) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            std::string name = entry->d_name;
            if (name != "." && name != "..") {
                result.push_back(name);
            }
        }
        closedir(dir);
    }
#endif
    
    return result;
}

bool createDirectory(const std::string& path) {
#ifdef _WIN32
    return _mkdir(path.c_str()) == 0 || GetLastError() == ERROR_ALREADY_EXISTS;
#else
    return mkdir(path.c_str(), 0755) == 0 || errno == EEXIST;
#endif
}

bool removeDirectory(const std::string& path) {
#ifdef _WIN32
    return _rmdir(path.c_str()) == 0;
#else
    return rmdir(path.c_str()) == 0;
#endif
}

bool removeFile(const std::string& path) {
#ifdef _WIN32
    return DeleteFileA(path.c_str()) != 0;
#else
    return unlink(path.c_str()) == 0;
#endif
}

bool copyFile(const std::string& from, const std::string& to) {
#ifdef _WIN32
    return CopyFileA(from.c_str(), to.c_str(), FALSE) != 0;
#else
    std::ifstream src(from, std::ios::binary);
    std::ofstream dst(to, std::ios::binary);
    
    if (!src || !dst) {
        return false;
    }
    
    dst << src.rdbuf();
    return src.good() && dst.good();
#endif
}

bool moveFile(const std::string& from, const std::string& to) {
#ifdef _WIN32
    return MoveFileA(from.c_str(), to.c_str()) != 0;
#else
    return rename(from.c_str(), to.c_str()) == 0;
#endif
}

std::string getEnv(const std::string& name) {
    const char* value = std::getenv(name.c_str());
    return value ? std::string(value) : "";
}

void setEnv(const std::string& name, const std::string& value) {
#ifdef _WIN32
    _putenv_s(name.c_str(), value.c_str());
#else
    setenv(name.c_str(), value.c_str(), 1);
#endif
}

int execute(const std::string& command) {
    return std::system(command.c_str());
}

void exitProcess(int code) {
    std::exit(code);
}

std::string getExecutablePath() {
#ifdef _WIN32
    char buffer[MAX_PATH];
    GetModuleFileNameA(NULL, buffer, MAX_PATH);
    return std::string(buffer);
#elif defined(__linux__)
    char buffer[4096];
    ssize_t count = readlink("/proc/self/exe", buffer, 4096);
    if (count != -1) {
        buffer[count] = '\0';
        return std::string(buffer);
    }
    return "";
#elif defined(__APPLE__)
    char buffer[4096];
    uint32_t size = sizeof(buffer);
    if (_NSGetExecutablePath(buffer, &size) == 0) {
        return std::string(buffer);
    }
    return "";
#else
    return "";
#endif
}

std::string getPlatform() {
#ifdef _WIN32
    return "Windows";
#elif defined(__APPLE__)
    return "macOS";
#elif defined(__linux__)
    return "Linux";
#elif defined(__FreeBSD__)
    return "FreeBSD";
#elif defined(__OpenBSD__)
    return "OpenBSD";
#elif defined(__NetBSD__)
    return "NetBSD";
#else
    return "Unknown";
#endif
}

std::string getArch() {
#if defined(__x86_64__) || defined(_M_X64)
    return "x86_64";
#elif defined(__i386__) || defined(_M_IX86)
    return "x86";
#elif defined(__aarch64__) || defined(_M_ARM64)
    return "arm64";
#elif defined(__arm__) || defined(_M_ARM)
    return "arm";
#elif defined(__powerpc64__)
    return "ppc64";
#elif defined(__powerpc__)
    return "ppc";
#else
    return "unknown";
#endif
}

std::string getUsername() {
#ifdef _WIN32
    char username[UNLEN + 1];
    DWORD username_len = UNLEN + 1;
    if (GetUserNameA(username, &username_len)) {
        return std::string(username);
    }
    return "";
#else
    struct passwd *pw = getpwuid(getuid());
    return pw ? std::string(pw->pw_name) : "";
#endif
}

std::string getHostname() {
#ifdef _WIN32
    char hostname[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD hostname_len = MAX_COMPUTERNAME_LENGTH + 1;
    if (GetComputerNameA(hostname, &hostname_len)) {
        return std::string(hostname);
    }
    return "";
#else
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        return std::string(hostname);
    }
    return "";
#endif
}

std::string joinPath(const std::string& a, const std::string& b) {
    if (a.empty()) return b;
    if (b.empty()) return a;
    
    char lastChar = a[a.length() - 1];
    bool hasSlash = (lastChar == '/' || lastChar == '\\');
    
    if (hasSlash) {
        return a + b;
    } else {
        return a + PATH_SEP + b;
    }
}

std::string getPathSeparator() {
    return PATH_SEP;
}

} // namespace platform
} // namespace neutron
