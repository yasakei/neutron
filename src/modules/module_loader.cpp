
#include "modules/module_loader.h"
#include <sstream>
#include <algorithm>

namespace neutron {

    std::vector<std::string> getUsedModules(const std::string& source) {
        std::vector<std::string> modules;
        std::stringstream ss(source);
        std::string line;

        while (std::getline(ss, line)) {
            // Remove carriage return for Windows line endings (\r\n)
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }

            std::stringstream line_ss(line);
            std::string keyword;
            line_ss >> keyword;

            if (keyword == "use") {
                std::string module_name;
                line_ss >> module_name;
                // remove trailing semicolon
                if (!module_name.empty() && module_name.back() == ';') {
                    module_name.pop_back();
                }
                // Remove any trailing \r that might have slipped through
                if (!module_name.empty() && module_name.back() == '\r') {
                    module_name.pop_back();
                }
                modules.push_back(module_name);
            }
        }

        return modules;
    }

}
