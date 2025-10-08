#ifndef BOX_INSTALLER_H
#define BOX_INSTALLER_H

#include "registry.h"
#include <string>
#include <vector>

namespace box {

/**
 * Module installer
 */
class Installer {
public:
    Installer();

    /**
     * Install a module
     * @param moduleName Name of the module to install
     * @param global Install globally (true) or locally (false)
     * @return true if successful
     */
    bool install(const std::string& moduleName, bool global = true);

    /**
     * Uninstall a module
     * @param moduleName Name of the module to uninstall
     * @param global Uninstall from global (true) or local (false)
     * @return true if successful
     */
    bool uninstall(const std::string& moduleName, bool global = true);

    /**
     * Update a module to latest version
     * @param moduleName Name of the module to update
     * @param global Update global (true) or local (false) installation
     * @return true if successful
     */
    bool update(const std::string& moduleName, bool global = true);

    /**
     * List installed modules
     * @param global List global (true) or local (false) modules
     * @return List of installed module names
     */
    std::vector<std::string> listInstalled(bool global = true);

    /**
     * Check if a module is installed
     * @param moduleName Name of the module
     * @param global Check global (true) or local (false)
     * @return true if installed
     */
    bool isInstalled(const std::string& moduleName, bool global = true);

    /**
     * Get installation directory
     * @param global Get global (true) or local (false) directory
     * @return Path to installation directory
     */
    std::string getInstallDir(bool global = true);

private:
    Registry registry;
    std::string globalModulesDir;  // ~/.box/modules/
    std::string localModulesDir;   // ./box/

    /**
     * Download and install module binary
     */
    bool downloadBinary(const ModuleMetadata& metadata, const std::string& installPath);

    /**
     * Install dependencies
     */
    bool installDependencies(const ModuleMetadata& metadata);

    /**
     * Verify module compatibility
     */
    bool verifyCompatibility(const ModuleMetadata& metadata);

    /**
     * Create directory if it doesn't exist
     */
    bool ensureDirectory(const std::string& path);

    /**
     * Download file from URL to path
     */
    bool downloadFile(const std::string& url, const std::string& outputPath);
};

} // namespace box

#endif // BOX_INSTALLER_H
