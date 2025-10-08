#ifndef BOX_PLATFORM_H
#define BOX_PLATFORM_H

#include <string>

namespace box {

/**
 * Platform detection and utilities
 */
class Platform {
public:
    enum class OS {
        LINUX,
        WINDOWS,
        MACOS,
        UNKNOWN
    };

    /**
     * Detect the current operating system
     */
    static OS detectOS();

    /**
     * Get the OS as a string
     */
    static std::string getOSString();

    /**
     * Get the shared library extension for current platform
     * @return ".so" for Linux, ".dll" for Windows, ".dylib" for macOS
     */
    static std::string getLibraryExtension();

    /**
     * Get the entry key for current platform
     * @return "entry-linux", "entry-win", or "entry-mac"
     */
    static std::string getEntryKey();

    /**
     * Check if running on Linux
     */
    static bool isLinux();

    /**
     * Check if running on Windows
     */
    static bool isWindows();

    /**
     * Check if running on macOS
     */
    static bool isMacOS();
};

} // namespace box

#endif // BOX_PLATFORM_H
