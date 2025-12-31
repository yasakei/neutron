/*
 * Neutron Programming Language - Project Builder
 * Copyright (c) 2026 yasakei
 */

#ifndef NEUTRON_PROJECT_BUILDER_H
#define NEUTRON_PROJECT_BUILDER_H

#include <string>
#include <vector>

namespace neutron {

class ProjectBuilder {
public:
    // Build a project to a native executable
    static bool buildProjectExecutable(
        const std::string& projectRoot,
        const std::string& sourceCode,
        const std::string& sourcePath,
        const std::string& outputPath,
        const std::string& neutronExecutablePath,
        bool bundleLibs = false
    );
    
private:
    static std::vector<std::string> findProjectModules(const std::string& projectRoot);
    static std::vector<std::string> findBoxModules(const std::string& projectRoot);
    static std::string readFile(const std::string& path);
    static bool fileExists(const std::string& path);
    static std::vector<std::string> getRuntimeSources();
    static std::vector<std::string> getBuiltinModuleSources();
};

} // namespace neutron

#endif // NEUTRON_PROJECT_BUILDER_H
