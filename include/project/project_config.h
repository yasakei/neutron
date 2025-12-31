/*
 * Neutron Programming Language - Project Configuration
 * Copyright (c) 2026 yasakei
 * 
 * This software is distributed under the Neutron Public License 1.0.
 */

#ifndef NEUTRON_PROJECT_CONFIG_H
#define NEUTRON_PROJECT_CONFIG_H

#include <string>
#include <vector>
#include <map>
#include <memory>

namespace neutron {

struct ProjectConfig {
    std::string name;
    std::string version;
    std::string entry;
    std::string author;
    std::string description;
    std::vector<std::string> dependencies;
    std::map<std::string, std::string> metadata;
    
    ProjectConfig() : name("neutron-project"), version("1.0.0"), entry("main.nt") {}
};

class ProjectConfigParser {
public:
    static std::unique_ptr<ProjectConfig> parse(const std::string& configPath);
    static bool save(const ProjectConfig& config, const std::string& configPath);
    static std::unique_ptr<ProjectConfig> createDefault(const std::string& projectName = "neutron-project");
    
private:
    static std::string trim(const std::string& str);
    static std::pair<std::string, std::string> parseLine(const std::string& line);
};

} // namespace neutron

#endif // NEUTRON_PROJECT_CONFIG_H
