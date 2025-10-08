#include "platform.h"

#ifdef _WIN32
    #define PLATFORM_WINDOWS
#elif defined(__APPLE__) && defined(__MACH__)
    #define PLATFORM_MACOS
#elif defined(__linux__)
    #define PLATFORM_LINUX
#endif

namespace box {

Platform::OS Platform::detectOS() {
#ifdef PLATFORM_WINDOWS
    return OS::WINDOWS;
#elif defined(PLATFORM_MACOS)
    return OS::MACOS;
#elif defined(PLATFORM_LINUX)
    return OS::LINUX;
#else
    return OS::UNKNOWN;
#endif
}

std::string Platform::getOSString() {
    switch (detectOS()) {
        case OS::LINUX:
            return "Linux";
        case OS::WINDOWS:
            return "Windows";
        case OS::MACOS:
            return "macOS";
        default:
            return "Unknown";
    }
}

std::string Platform::getLibraryExtension() {
    switch (detectOS()) {
        case OS::LINUX:
            return ".so";
        case OS::WINDOWS:
            return ".dll";
        case OS::MACOS:
            return ".dylib";
        default:
            return ".so";
    }
}

std::string Platform::getEntryKey() {
    switch (detectOS()) {
        case OS::LINUX:
            return "entry-linux";
        case OS::WINDOWS:
            return "entry-win";
        case OS::MACOS:
            return "entry-mac";
        default:
            return "entry-linux";
    }
}

bool Platform::isLinux() {
    return detectOS() == OS::LINUX;
}

bool Platform::isWindows() {
    return detectOS() == OS::WINDOWS;
}

bool Platform::isMacOS() {
    return detectOS() == OS::MACOS;
}

} // namespace box
