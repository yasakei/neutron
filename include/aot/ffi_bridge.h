#ifndef NEUTRON_AOT_FFI_BRIDGE_H
#define NEUTRON_AOT_FFI_BRIDGE_H

#include <string>
#include <vector>
#include <cstdint>

namespace neutron {
namespace aot {

// Represents a C function type mapped for QBE
enum class CType {
    VOID,
    BOOL,
    INT8,
    INT16,
    INT32,   // -> QBE w
    INT64,   // -> QBE l
    UINT8,
    UINT16,
    UINT32,
    UINT64,
    FLOAT,   // -> QBE s
    DOUBLE,  // -> QBE d
    POINTER, // -> QBE l
    STRING,  // -> QBE l (char*)
    UNKNOWN
};

struct CFunction {
    std::string name;
    CType return_type;
    std::vector<CType> param_types;
    std::vector<std::string> param_names;
    bool is_extern_c;     // declared with extern "C"
    std::string header;   // source header path
};

struct CHeaderScanResult {
    std::string header_path;
    std::vector<CFunction> functions;
    std::string error_msg;
    bool success;
};

class FFIBridge {
public:
    // Parse a C header file and extract exported function signatures
    // Uses libclang if available, otherwise falls back to manual parsing
    static CHeaderScanResult scan_header(const std::string& header_path,
                                          const std::vector<std::string>& include_dirs = {});

    // Manual declaration parsing (no libclang dependency)
    // Parses simple C function declarations from a string
    static std::vector<CFunction> parse_declarations(const std::string& source);

    // Map C type string to CType enum
    static CType type_from_string(const std::string& type_str);

    // Map CType to QBE type character (w, l, d, s, b)
    static std::string type_to_qbe(CType t);

    // Map CType to Neutron type tag
    static int type_to_tag(CType t);

    // Check if libclang is available at runtime
    static bool has_libclang();

private:
    // Minimal C declaration parser helpers
    static std::string trim(const std::string& s);
    static std::vector<std::string> split(const std::string& s, char delim);
};

} // namespace aot
} // namespace neutron

#endif // NEUTRON_AOT_FFI_BRIDGE_H