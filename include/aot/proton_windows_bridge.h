#ifndef NEUTRON_PROTON_WINDOWS_BRIDGE_H
#define NEUTRON_PROTON_WINDOWS_BRIDGE_H

#include <string>

namespace neutron {
namespace aot {

struct ProtonWindowsBridge {
    static bool assemble_object(const std::string& asm_path, const std::string& obj_path);
    static bool find_assembler(std::string& out_assembler);
    static std::string proton_lib_name();
    static std::string object_extension();
    static bool is_windows_msvc();
};

} // namespace aot
} // namespace neutron

#endif // NEUTRON_PROTON_WINDOWS_BRIDGE_H
