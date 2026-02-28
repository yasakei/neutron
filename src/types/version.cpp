#include "types/version.h"
#include <sstream>
#include <ctime>

namespace neutron {

std::string Version::getVersion() {
    std::ostringstream oss;
    oss << MAJOR << "." << MINOR << "." << PATCH << "-" << STAGE;
    return oss.str();
}

std::string Version::getFullVersion() {
    std::ostringstream oss;
    oss << "Neutron " << getVersion() << " (" << getPlatform() << ")";
    return oss.str();
}

std::string Version::getMajorMinorPatch() {
    std::ostringstream oss;
    oss << MAJOR << "." << MINOR << "." << PATCH;
    return oss.str();
}

std::string Version::getBuildDate() {
    return __DATE__ " " __TIME__;
}

std::string Version::getPlatform() {
#if defined(_WIN32)
    #if defined(__MINGW64__)
        return "Windows MINGW64";
    #elif defined(__MINGW32__)
        return "Windows MINGW32";
    #else
        return "Windows MSVC";
    #endif
#elif defined(__APPLE__)
    return "macOS";
#elif defined(__linux__)
    return "Linux";
#else
    return "Unknown";
#endif
}

} // namespace neutron
