//v26.2.2
#ifndef NEUTRON_VERSION_H
#define NEUTRON_VERSION_H

#include <string>

namespace neutron {

class Version {
public:
    // Version components
    static constexpr int MAJOR = 26;
    static constexpr int MINOR = 2;
    static constexpr int PATCH = 2;
    static constexpr const char* STAGE = "stable";
    
    // Full version string
    static std::string getVersion();
    
    // Detailed version info
    static std::string getFullVersion();
    
    // Version components as string
    static std::string getMajorMinorPatch();
    
    // Build information
    static std::string getBuildDate();
    static std::string getPlatform();
};

} // namespace neutron

#endif // NEUTRON_VERSION_H
