#ifndef BOX_BUILDER_H
#define BOX_BUILDER_H

#include <string>
#include <vector>

namespace box {

/**
 * Native module builder
 */
class Builder {
public:
    Builder();

    /**
     * Build native module for current platform
     * @param moduleName Name of the module to build
     * @param sourcePath Path to module source directory
     * @param outputDir Output directory for built binary (box/)
     * @param version Module version
     * @return true if successful
     */
    bool buildNative(const std::string& moduleName, 
                     const std::string& sourcePath,
                     const std::string& outputDir,
                     const std::string& version);

    /**
     * Build Neutron source module (future feature)
     * @param moduleName Name of the module to build
     * @param sourcePath Path to module source
     * @param outputDir Output directory
     * @return true if successful
     */
    bool buildNeutron(const std::string& moduleName,
                      const std::string& sourcePath,
                      const std::string& outputDir);

    /**
     * Get compiler command for current platform
     * @return Compiler command (g++, clang++, cl)
     */
    std::string getCompiler();

    /**
     * Get linker flags for shared library
     * @return Platform-specific linker flags
     */
    std::vector<std::string> getLinkerFlags();

    /**
     * Get include paths for Neutron headers
     * @return List of include directories
     */
    std::vector<std::string> getIncludePaths();

private:
    /**
     * Execute build command
     */
    bool executeCommand(const std::string& command);

    /**
     * Find Neutron installation directory
     */
    std::string findNeutronDir();

    /**
     * Generate build command
     */
    std::string generateBuildCommand(const std::string& moduleName,
                                     const std::string& sourcePath,
                                     const std::string& outputPath);
    
    /**
     * Create metadata.json for the module
     */
    bool createMetadata(const std::string& moduleName,
                       const std::string& version,
                       const std::string& outputDir);
};

} // namespace box

#endif // BOX_BUILDER_H
