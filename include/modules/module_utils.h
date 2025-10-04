#ifndef NEUTRON_MODULE_UTILS_H
#define NEUTRON_MODULE_UTILS_H

#include "vm.h"
#include <string>

namespace neutron {
    namespace module_utils {
        // Execute a Neutron code string within a module context
        Value execute_neutron_code(VM* vm, const std::string& code);
        
        // Execute a Neutron code string and return the result as a string
        std::string execute_neutron_code_string(VM* vm, const std::string& code);
        
        // Execute a Neutron code string and return the result as a number
        double execute_neutron_code_number(VM* vm, const std::string& code);
        
        // Execute a Neutron code string and return the result as a boolean
        bool execute_neutron_code_boolean(VM* vm, const std::string& code);
    }
}

#endif // NEUTRON_MODULE_UTILS_H