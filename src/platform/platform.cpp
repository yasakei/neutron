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
