#include "registry.h"
#include "platform.h"
#include <iostream>
#include <sstream>
#include <algorithm>

// For HTTP requests - will need to link with curl or similar
#ifdef _WIN32
    #include <windows.h>
    #include <wininet.h>
    #pragma comment(lib, "wininet.lib")
#else
    #include <curl/curl.h>
#endif

namespace box {

// Callback for curl to write data
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    userp->append((char*)contents, size * nmemb);
    return size * nmemb;
}

Registry::Registry() {
    registryURL = "https://raw.githubusercontent.com/neutron-modules/nur/refs/heads/main";
}

std::string Registry::download(const std::string& url) {
    std::string response;
    
#ifdef _WIN32
    // Windows implementation using WinINet
    HINTERNET hInternet = InternetOpenA("Box/1.0", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (hInternet) {
        HINTERNET hUrl = InternetOpenUrlA(hInternet, url.c_str(), NULL, 0, INTERNET_FLAG_RELOAD, 0);
        if (hUrl) {
            char buffer[4096];
            DWORD bytesRead;
            while (InternetReadFile(hUrl, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
                response.append(buffer, bytesRead);
            }
            InternetCloseHandle(hUrl);
        }
        InternetCloseHandle(hInternet);
    }
#else
    // Unix-like systems using libcurl
    CURL* curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
        
        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
            response.clear();
        }
        curl_easy_cleanup(curl);
    }
#endif
    
    return response;
}

bool Registry::fetchIndex() {
    std::string indexURL = registryURL + "/nur.json";
    std::cout << "Fetching NUR index from " << indexURL << "..." << std::endl;
    
    std::string content = download(indexURL);
    if (content.empty()) {
        std::cerr << "Failed to fetch NUR index" << std::endl;
        return false;
    }
    
    return parseIndex(content);
}

bool Registry::parseIndex(const std::string& content) {
    // Simple JSON parsing for nur.json
    // Format: {"version":"1.0","modules":{"base64":"./modules/base64.json",...}}
    
    moduleIndex.clear();
    
    // Find "modules" object
    size_t modulesPos = content.find("\"modules\"");
    if (modulesPos == std::string::npos) {
        std::cerr << "Invalid NUR index format: 'modules' not found" << std::endl;
        return false;
    }
    
    // Find opening brace of modules object
    size_t openBrace = content.find('{', modulesPos);
    if (openBrace == std::string::npos) return false;
    
    // Parse module entries
    size_t pos = openBrace + 1;
    while (pos < content.length()) {
        // Find module name
        size_t nameStart = content.find('"', pos);
        if (nameStart == std::string::npos) break;
        
        size_t nameEnd = content.find('"', nameStart + 1);
        if (nameEnd == std::string::npos) break;
        
        std::string moduleName = content.substr(nameStart + 1, nameEnd - nameStart - 1);
        
        // Find module URL
        size_t urlStart = content.find('"', nameEnd + 1);
        if (urlStart == std::string::npos) break;
        
        size_t urlEnd = content.find('"', urlStart + 1);
        if (urlEnd == std::string::npos) break;
        
        std::string moduleURL = content.substr(urlStart + 1, urlEnd - urlStart - 1);
        
        // Convert relative URL to absolute
        if (moduleURL[0] == '.') {
            moduleURL = registryURL + moduleURL.substr(1);
        }
        
        moduleIndex[moduleName] = moduleURL;
        
        pos = urlEnd + 1;
        
        // Check for end of modules object
        if (content.find('}', pos) < content.find('"', pos)) break;
    }
    
    std::cout << "Loaded " << moduleIndex.size() << " modules from NUR" << std::endl;
    return !moduleIndex.empty();
}

std::string Registry::getModuleURL(const std::string& moduleName) {
    auto it = moduleIndex.find(moduleName);
    if (it != moduleIndex.end()) {
        return it->second;
    }
    return "";
}

ModuleMetadata Registry::fetchModuleMetadata(const std::string& moduleName) {
    ModuleMetadata metadata;
    metadata.name = moduleName;
    
    std::string moduleURL = getModuleURL(moduleName);
    if (moduleURL.empty()) {
        std::cerr << "Module not found in registry: " << moduleName << std::endl;
        return metadata;
    }
    
    std::cout << "Fetching metadata for " << moduleName << "..." << std::endl;
    std::string content = download(moduleURL);
    
    if (content.empty()) {
        std::cerr << "Failed to fetch module metadata" << std::endl;
        return metadata;
    }
    
    // Parse module metadata JSON with versions
    // Format: {"name":"base64","latest":"1.0.1","versions":{"1.0.0":{...},"1.0.1":{...}}}
    
    auto extractValue = [&content](const std::string& key, size_t startPos = 0) -> std::string {
        std::string searchKey = "\"" + key + "\"";
        size_t pos = content.find(searchKey, startPos);
        if (pos == std::string::npos) return "";
        
        size_t colonPos = content.find(':', pos);
        if (colonPos == std::string::npos) return "";
        
        size_t valueStart = content.find('"', colonPos);
        if (valueStart == std::string::npos) return "";
        
        size_t valueEnd = content.find('"', valueStart + 1);
        if (valueEnd == std::string::npos) return "";
        
        return content.substr(valueStart + 1, valueEnd - valueStart - 1);
    };
    
    metadata.description = extractValue("description");
    metadata.author = extractValue("author");
    metadata.license = extractValue("license");
    metadata.repository = extractValue("repository");
    metadata.latest = extractValue("latest");
    
    // Parse versions object
    size_t versionsPos = content.find("\"versions\"");
    if (versionsPos != std::string::npos) {
        size_t versionObjStart = content.find('{', versionsPos);
        if (versionObjStart != std::string::npos) {
            // Find each version key
            size_t pos = versionObjStart + 1;
            size_t endOfVersions = content.length();
            
            // Find the closing brace for versions object (simple brace counting)
            int braceCount = 1;
            for (size_t i = versionObjStart + 1; i < content.length() && braceCount > 0; i++) {
                if (content[i] == '{') braceCount++;
                else if (content[i] == '}') {
                    braceCount--;
                    if (braceCount == 0) {
                        endOfVersions = i;
                        break;
                    }
                }
            }
            
            while (pos < endOfVersions) {
                size_t versionKeyStart = content.find('"', pos);
                if (versionKeyStart == std::string::npos || versionKeyStart >= endOfVersions) break;
                
                size_t versionKeyEnd = content.find('"', versionKeyStart + 1);
                if (versionKeyEnd == std::string::npos) break;
                
                std::string versionNum = content.substr(versionKeyStart + 1, versionKeyEnd - versionKeyStart - 1);
                
                // Parse this version's metadata
                size_t versionObjStart2 = content.find('{', versionKeyEnd);
                if (versionObjStart2 == std::string::npos) break;
                
                VersionMetadata versionMeta;
                versionMeta.description = extractValue("description", versionObjStart2);
                versionMeta.entryLinux = extractValue("entry-linux", versionObjStart2);
                versionMeta.entryWin = extractValue("entry-win", versionObjStart2);
                versionMeta.entryMac = extractValue("entry-mac", versionObjStart2);
                
                metadata.versions[versionNum] = versionMeta;
                
                // Move to next version - find matching closing brace
                int versionBraceCount = 1;
                pos = versionObjStart2 + 1;
                while (pos < endOfVersions && versionBraceCount > 0) {
                    if (content[pos] == '{') versionBraceCount++;
                    else if (content[pos] == '}') versionBraceCount--;
                    pos++;
                }
            }
        }
    }
    
    return metadata;
}

std::vector<std::string> Registry::search(const std::string& query) {
    std::vector<std::string> results;
    std::string lowerQuery = query;
    std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(), ::tolower);
    
    for (const auto& pair : moduleIndex) {
        std::string lowerName = pair.first;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
        
        if (lowerName.find(lowerQuery) != std::string::npos) {
            results.push_back(pair.first);
        }
    }
    
    return results;
}

std::vector<std::string> Registry::listModules() {
    std::vector<std::string> modules;
    for (const auto& pair : moduleIndex) {
        modules.push_back(pair.first);
    }
    return modules;
}

} // namespace box
