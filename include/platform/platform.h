#pragma once
#include <string>
#include <vector>

namespace neutron {
namespace platform {

// File operations
std::string getCwd();
bool setCwd(const std::string& path);
bool fileExists(const std::string& path);
bool isDirectory(const std::string& path);
std::vector<std::string> listDirectory(const std::string& path);
bool createDirectory(const std::string& path);
bool removeDirectory(const std::string& path);
bool removeFile(const std::string& path);
bool copyFile(const std::string& from, const std::string& to);
bool moveFile(const std::string& from, const std::string& to);

// Process operations
std::string getEnv(const std::string& name);
void setEnv(const std::string& name, const std::string& value);
int execute(const std::string& command);
void exitProcess(int code);
std::string getExecutablePath();

// System info
std::string getPlatform();
std::string getArch();
std::string getUsername();
std::string getHostname();

// Path operations
std::string joinPath(const std::string& a, const std::string& b);
std::string getPathSeparator();

} // namespace platform
} // namespace neutron
