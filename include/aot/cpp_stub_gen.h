#ifndef NEUTRON_AOT_CPP_STUB_GEN_H
#define NEUTRON_AOT_CPP_STUB_GEN_H

#include "aot/ffi_bridge.h"
#include <string>
#include <vector>

namespace neutron {
namespace aot {

class CppStubGen {
public:
    // Generate C++ stub source from parsed C function declarations
    // The stub provides QBE-callable wrappers around native C functions.
    // Each wrapper:
    //   1. Takes QBE-style (tag, data) pairs
    //   2. Converts to C types
    //   3. Calls the original function
    //   4. Converts result back to (tag, data) in rt_ret
    static std::string generate_stubs(const std::vector<CFunction>& functions,
                                       const std::string& module_name = "ffi");

    // Generate a complete .cpp file with includes and stubs
    static std::string generate_source(const std::vector<CFunction>& functions,
                                        const std::string& module_name,
                                        const std::vector<std::string>& extra_includes = {});

    // Write stub source to file
    static bool write_stub_file(const std::string& path,
                                 const std::vector<CFunction>& functions,
                                 const std::string& module_name);

private:
    static std::string generate_param_conversion(const CFunction& func, int param_idx);
    static std::string generate_return_conversion(const CFunction& func, const std::string& result_var);
    static std::string ctype_to_cpp_type(CType t);
    static std::string ctype_to_default_value(CType t);
};

} // namespace aot
} // namespace neutron

#endif // NEUTRON_AOT_CPP_STUB_GEN_H