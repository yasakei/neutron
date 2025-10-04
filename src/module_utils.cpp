#include "module_utils.h"
#include <stdexcept>

namespace neutron {
    namespace module_utils {
        Value execute_neutron_code(VM* vm, const std::string& code) {
            return vm->execute_string(code);
        }
        
        std::string execute_neutron_code_string(VM* vm, const std::string& code) {
            Value result = vm->execute_string(code);
            return result.toString();
        }
        
        double execute_neutron_code_number(VM* vm, const std::string& code) {
            Value result = vm->execute_string(code);
            if (result.type == ValueType::NUMBER) {
                return std::get<double>(result.as);
            }
            throw std::runtime_error("Expected number result from Neutron code execution");
        }
        
        bool execute_neutron_code_boolean(VM* vm, const std::string& code) {
            Value result = vm->execute_string(code);
            if (result.type == ValueType::BOOLEAN) {
                return std::get<bool>(result.as);
            }
            throw std::runtime_error("Expected boolean result from Neutron code execution");
        }
    }
}