#ifndef NEUTRON_AOT_NTM_METADATA_H
#define NEUTRON_AOT_NTM_METADATA_H

#include "types/value.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

namespace neutron {
namespace aot {

enum class NtmType {
    NIL,
    BOOL,
    NUMBER,
    STRING,
    ARRAY,
    OBJECT,
    CLASS,
    INSTANCE,
    CALLABLE,
    ANY
};

struct NtmParam {
    std::string name;
    NtmType type;
};

struct NtmExportFunc {
    std::string name;
    std::string mangled_name;  // module$func_name
    std::vector<NtmParam> params;
    NtmType return_type;
};

struct NtmExportConst {
    std::string name;
    std::string mangled_name;
    NtmType type;
    std::string value_str;  // serialized value
};

struct NtmModule {
    std::string module_name;
    std::vector<NtmExportFunc> functions;
    std::vector<NtmExportConst> constants;
};

class NtmMetadata {
public:
    // Serialize module metadata to string (.ntm format)
    static std::string serialize(const NtmModule& module);

    // Deserialize from string
    static NtmModule deserialize(const std::string& data);

    // Read .ntm file from disk
    static NtmModule read_file(const std::string& path);

    // Write .ntm file to disk
    static bool write_file(const std::string& path, const NtmModule& module);

    // Mangle a symbol name with module prefix
    static std::string mangle(const std::string& module_name, const std::string& symbol);

    // Demangle: extract module and symbol name
    static bool demangle(const std::string& mangled, std::string& module_name, std::string& symbol);

    // Map Neutron ValueType to NtmType
    static NtmType value_type_to_ntm(ValueType vt);

    // Map NtmType to QBE type string
    static std::string ntm_to_qbe_type(NtmType t);

private:
    static std::string escape(const std::string& s);
    static std::string unescape(const std::string& s);
};

} // namespace aot
} // namespace neutron

#endif // NEUTRON_AOT_NTM_METADATA_H