#include "platform.h"

// Platform detection macros (only define if not already defined)
#ifdef _WIN32
    #ifndef PLATFORM_WINDOWS
    #define PLATFORM_WINDOWS
    #endif
#elif defined(__APPLE__) && defined(__MACH__)
    #ifndef PLATFORM_MACOS
    #define PLATFORM_MACOS
    #endif
#elif defined(__linux__)
    #ifndef PLATFORM_LINUX
    #define PLATFORM_LINUX
    #endif
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
