#ifndef BOX_REGISTRY_H
#define BOX_REGISTRY_H

#include <string>
#include <map>
#include <vector>

namespace box {

/**
 * Version-specific metadata
 */
struct VersionMetadata {
    std::string description;
    std::string entryLinux;
    std::string entryWin;
    std::string entryMac;
    std::map<std::string, std::string> deps;
};

/**
 * Module metadata from module.json
 */
struct ModuleMetadata {
    std::string name;
    std::string description;
    std::string author;
    std::string license;
    std::string repository;
    std::string latest;
    std::map<std::string, VersionMetadata> versions;
};

/**
 * NUR (Neutron User Repository) registry client
 */
class Registry {
public:
    Registry();

    /**
     * Fetch the NUR index (nur.json)
     * @return true if successful
     */
    bool fetchIndex();

    /**
     * Get module metadata URL from the index
     * @param moduleName Name of the module
     * @return URL to module.json or empty string if not found
     */
    std::string getModuleURL(const std::string& moduleName);

    /**
     * Fetch module metadata
     * @param moduleName Name of the module
     * @return ModuleMetadata structure
     */
    ModuleMetadata fetchModuleMetadata(const std::string& moduleName);

    /**
     * Search for modules by name
     * @param query Search query
     * @return List of matching module names
     */
    std::vector<std::string> search(const std::string& query);

    /**
     * List all available modules
     * @return List of all module names
     */
    std::vector<std::string> listModules();

    /**
     * Download content from URL
     * @param url URL to download from
     * @return Downloaded content or empty string on failure
     */
    std::string download(const std::string& url);

private:
    std::string registryURL;
    std::map<std::string, std::string> moduleIndex; // name -> metadata URL

    /**
     * Parse nur.json content
     */
    bool parseIndex(const std::string& content);
};

} // namespace box

#endif // BOX_REGISTRY_H
