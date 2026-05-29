#ifndef NEUTRON_AOT_BUILDER_H
#define NEUTRON_AOT_BUILDER_H

#include "compiler/bytecode.h"
#include "aot/qbe_codegen.h"
#include <string>
#include <vector>

namespace neutron {
namespace aot {

class AotBuilder {
public:
    // Always available — QBE is embedded as Proton
    static bool detect_qbe() {
        return true;
    }

    // Generate QBE IR from a chunk and return as string
    static std::string generate_qbe_ir(const Chunk* chunk, const std::string& func_name);

    // Write QBE IR to a file (legacy, for debugging)
    static bool write_qbe_ir(const Chunk* chunk, const std::string& func_name,
                              const std::string& ssa_path);

    // Compile SSA string directly to object file using embedded Proton
    static bool compile_ssa(const std::string& ssa, const std::string& obj_path);

    // Assemble QBE IR (.ssa) to object file (.o) via external qbe (legacy)
    static bool assemble(const std::string& ssa_path, const std::string& obj_path);

    // Full pipeline: bytecode -> QBE IR -> .o
    static std::string compile_to_object(const Chunk* chunk, const std::string& func_name,
                                          const std::string& output_dir);

    // Link object file with runtime library to produce executable
    static bool link(const std::string& obj_path, const std::string& output_path,
                     const std::string& runtime_lib_path, bool is_windows);
};

} // namespace aot
} // namespace neutron

#endif // NEUTRON_AOT_BUILDER_H
