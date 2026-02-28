/*
 * Neutron Programming Language - Project Manager
 * Copyright (c) 2026 yasakei
 */

#ifndef NEUTRON_PROJECT_MANAGER_H
#define NEUTRON_PROJECT_MANAGER_H

#include <string>
#include <memory>
#include "project/project_config.h"

namespace neutron {

class ProjectManager {
public:
    // Initialize a new project in the current directory
    static bool initProject(const std::string& projectName = "");
    
    // Build the project to a native executable
    static bool buildProject(const std::string& projectRoot = ".");
    
    // Run the project
    static bool runProject(const std::string& projectRoot = ".");
    
    // Install Box package manager
    static bool installBox(const std::string& projectRoot = ".");
    
    // Check if current directory is a Neutron project
    static bool isNeutronProject(const std::string& path = ".");
    
    // Find project root from current directory
    static std::string findProjectRoot(const std::string& startPath = ".");
    
    // Load project configuration
    static std::unique_ptr<ProjectConfig> loadConfig(const std::string& projectRoot = ".");
    
private:
    static std::string getCurrentDirectory();
    static bool createDirectory(const std::string& path);
    static bool fileExists(const std::string& path);
    static bool directoryExists(const std::string& path);
    static int executeCommand(const std::string& command);
    static std::string getBoxPath(const std::string& projectRoot);
};

} // namespace neutron

#endif // NEUTRON_PROJECT_MANAGER_H
