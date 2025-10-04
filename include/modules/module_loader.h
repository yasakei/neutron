
#ifndef NEUTRON_MODULE_LOADER_H
#define NEUTRON_MODULE_LOADER_H

#include <string>
#include <vector>

namespace neutron {
    std::vector<std::string> getUsedModules(const std::string& source);
}

#endif // NEUTRON_MODULE_LOADER_H
