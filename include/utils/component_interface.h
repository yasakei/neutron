#pragma once

#include <memory>
#include <string>

namespace neutron {

class VM;  // Forward declaration

/**
 * @brief Interface for all Neutron components that can be dynamically loaded
 * This provides a standard way to add new functionality to Neutron
 */
class ComponentInterface {
public:
    virtual ~ComponentInterface() = default;
    
    /**
     * @brief Initialize the component with the given VM
     * @param vm Pointer to the VM instance
     */
    virtual void initialize(VM* vm) = 0;
    
    /**
     * @brief Get the name of the component
     * @return Component name
     */
    virtual std::string getName() const = 0;
    
    /**
     * @brief Get the version of the component
     * @return Component version
     */
    virtual std::string getVersion() const = 0;
    
    /**
     * @brief Check if the component is compatible with the current system
     * @return true if compatible, false otherwise
     */
    virtual bool isCompatible() const = 0;
};

/**
 * @brief Base class for core features
 */
class CoreFeature : public ComponentInterface {
protected:
    std::string name;
    std::string version;

public:
    CoreFeature(const std::string& name, const std::string& version = "1.0.0");
    
    virtual std::string getName() const override;
    virtual std::string getVersion() const override;
    virtual bool isCompatible() const override;
};

} // namespace neutron