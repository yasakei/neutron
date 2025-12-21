#include "utils/component_interface.h"

namespace neutron {

CoreFeature::CoreFeature(const std::string& name, const std::string& version)
    : name(name), version(version) {
}

std::string CoreFeature::getName() const {
    return name;
}

std::string CoreFeature::getVersion() const {
    return version;
}

bool CoreFeature::isCompatible() const {
    // Basic compatibility check - this could be expanded
    return true;
}

} // namespace neutron