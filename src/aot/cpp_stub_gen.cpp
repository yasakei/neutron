#include "aot/cpp_stub_gen.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>

namespace neutron {
namespace aot {

std::string CppStubGen::ctype_to_cpp_type(CType t) {
    switch (t) {
        case CType::VOID:   return "void";
        case CType::BOOL:   return "bool";
        case CType::INT8:   return "int8_t";
        case CType::INT16:  return "int16_t";
        case CType::INT32:  return "int32_t";
        case CType::INT64:  return "int64_t";
        case CType::UINT8:  return "uint8_t";
        case CType::UINT16: return "uint16_t";
        case CType::UINT32: return "uint32_t";
        case CType::UINT64: return "uint64_t";
        case CType::FLOAT:  return "float";
        case CType::DOUBLE: return "double";
        case CType::POINTER:return "void*";
        case CType::STRING: return "const char*";
        default:            return "void*";
    }
}

std::string CppStubGen::ctype_to_default_value(CType t) {
    switch (t) {
        case CType::VOID:   return "";
        case CType::BOOL:   return "false";
        case CType::INT8:
        case CType::INT16:
        case CType::INT32:
        case CType::INT64:
        case CType::UINT8:
        case CType::UINT16:
        case CType::UINT32:
        case CType::UINT64: return "0";
        case CType::FLOAT:
        case CType::DOUBLE: return "0.0";
        case CType::POINTER:
        case CType::STRING: return "nullptr";
        default:            return "nullptr";
    }
}

std::string CppStubGen::generate_param_conversion(const CFunction& func, int param_idx) {
    if (param_idx < 0 || static_cast<size_t>(param_idx) >= func.param_types.size()) {
        return "";
    }

    CType t = func.param_types[param_idx];
    std::string param_name = "param" + std::to_string(param_idx);
    std::string tag_var = "tag_" + std::to_string(param_idx);
    std::string data_var = "data_" + std::to_string(param_idx);

    std::ostringstream code;
    code << "    // Convert parameter " << param_idx << " (" << func.param_names[param_idx] << ")\n";
    code << "    uint64_t " << tag_var << " = va_arg(args, uint64_t);\n";
    code << "    uint64_t " << data_var << " = va_arg(args, uint64_t);\n";
    code << "    " << ctype_to_cpp_type(t) << " " << param_name;

    switch (t) {
        case CType::BOOL:
            code << " = (" << data_var << " != 0);\n";
            break;
        case CType::INT8:
            code << " = static_cast<int8_t>(" << data_var << ");\n";
            break;
        case CType::INT16:
            code << " = static_cast<int16_t>(" << data_var << ");\n";
            break;
        case CType::INT32:
            code << " = static_cast<int32_t>(" << data_var << ");\n";
            break;
        case CType::INT64:
            code << " = static_cast<int64_t>(" << data_var << ");\n";
            break;
        case CType::UINT8:
            code << " = static_cast<uint8_t>(" << data_var << ");\n";
            break;
        case CType::UINT16:
            code << " = static_cast<uint16_t>(" << data_var << ");\n";
            break;
        case CType::UINT32:
            code << " = static_cast<uint32_t>(" << data_var << ");\n";
            break;
        case CType::UINT64:
            code << " = " << data_var << ";\n";
            break;
        case CType::FLOAT: {
            code << ";\n";
            code << "    double dbl_val_" << param_idx << ";\n";
            code << "    memcpy(&dbl_val_" << param_idx << ", &" << data_var << ", sizeof(double));\n";
            code << "    " << param_name << " = static_cast<float>(dbl_val_" << param_idx << ");\n";
            break;
        }
        case CType::DOUBLE: {
            code << ";\n";
            code << "    memcpy(&" << param_name << ", &" << data_var << ", sizeof(double));\n";
            break;
        }
        case CType::POINTER:
            code << " = reinterpret_cast<void*>(" << data_var << ");\n";
            break;
        case CType::STRING:
            code << " = reinterpret_cast<const char*>(" << data_var << ");\n";
            break;
        default:
            code << " = 0;\n";
            break;
    }

    return code.str();
}

std::string CppStubGen::generate_return_conversion(const CFunction& func, const std::string& result_var) {
    CType rt = func.return_type;
    std::ostringstream code;

    if (rt == CType::VOID) {
        code << "    // void return — store nil\n";
        code << "    rt_ret[0] = 1; // NIL\n";
        code << "    rt_ret[1] = 0;\n";
        return code.str();
    }

    code << "    // Convert return value\n";
    switch (rt) {
        case CType::BOOL:
            code << "    rt_ret[0] = 2; // BOOL\n";
            code << "    rt_ret[1] = " << result_var << " ? 1 : 0;\n";
            break;
        case CType::INT8:
        case CType::INT16:
        case CType::INT32:
        case CType::UINT8:
        case CType::UINT16:
        case CType::UINT32:
            code << "    rt_ret[0] = 3; // NUMBER\n";
            code << "    double __dbl_ret = static_cast<double>(" << result_var << ");\n";
            code << "    memcpy(&rt_ret[1], &__dbl_ret, sizeof(double));\n";
            break;
        case CType::INT64:
        case CType::UINT64:
            code << "    rt_ret[0] = 3; // NUMBER\n";
            code << "    double __dbl_ret = static_cast<double>(" << result_var << ");\n";
            code << "    memcpy(&rt_ret[1], &__dbl_ret, sizeof(double));\n";
            break;
        case CType::FLOAT:
            code << "    rt_ret[0] = 3; // NUMBER\n";
            code << "    double __dbl_ret = static_cast<double>(" << result_var << ");\n";
            code << "    memcpy(&rt_ret[1], &__dbl_ret, sizeof(double));\n";
            break;
        case CType::DOUBLE:
            code << "    rt_ret[0] = 3; // NUMBER\n";
            code << "    memcpy(&rt_ret[1], &" << result_var << ", sizeof(double));\n";
            break;
        case CType::POINTER:
            code << "    rt_ret[0] = 8; // INSTANCE (opaque pointer)\n";
            code << "    rt_ret[1] = reinterpret_cast<uint64_t>(" << result_var << ");\n";
            break;
        case CType::STRING:
            code << "    rt_ret[0] = 4; // STRING\n";
            code << "    rt_ret[1] = reinterpret_cast<uint64_t>(" << result_var << ");\n";
            break;
        default:
            code << "    rt_ret[0] = 1; // NIL\n";
            code << "    rt_ret[1] = 0;\n";
            break;
    }

    return code.str();
}

std::string CppStubGen::generate_stubs(const std::vector<CFunction>& functions,
                                        const std::string& module_name) {
    std::ostringstream code;

    code << "// Auto-generated C++ FFI stubs for module: " << module_name << "\n";
    code << "// Each stub wraps a C function for QBE AOT calling convention\n";
    code << "// Parameters arrive as (tag, data) pairs via variadic args\n";
    code << "// Results are stored in extern \"C\" rt_ret[2] buffer\n";
    code << "\n";
    code << "#include <cstdint>\n";
    code << "#include <cstring>\n";
    code << "#include <cstdarg>\n";
    code << "\n";

    // Declare the return buffer
    code << "extern \"C\" uint64_t rt_ret[2];\n";
    code << "\n";

    for (const auto& func : functions) {
        if (!func.is_extern_c) continue;

        code << "// Wrapper for " << func.name << "\n";
        code << "extern \"C\" void $" << module_name << "$" << func.name
             << "(uint64_t arg_count, ...) {\n";

        if (func.param_types.empty()) {
            code << "    (void)arg_count;\n";
        }

        code << "    va_list args;\n";
        code << "    va_start(args, arg_count);\n\n";

        // Generate parameter conversions
        for (size_t i = 0; i < func.param_types.size(); i++) {
            code << generate_param_conversion(func, i);
        }

        code << "    va_end(args);\n\n";

        // Generate the actual function call
        code << "    // Call " << func.name << "(";
        for (size_t i = 0; i < func.param_types.size(); i++) {
            if (i > 0) code << ", ";
            code << "param" << i;
        }
        code << ")\n";

        if (func.return_type != CType::VOID) {
            code << "    " << ctype_to_cpp_type(func.return_type) << " result = ";
        }
        code << "    " << func.name << "(";
        for (size_t i = 0; i < func.param_types.size(); i++) {
            if (i > 0) code << ", ";
            code << "param" << i;
        }
        code << ");\n\n";

        code << generate_return_conversion(func, "result");
        code << "}\n\n";
    }

    return code.str();
}

std::string CppStubGen::generate_source(const std::vector<CFunction>& functions,
                                         const std::string& module_name,
                                         const std::vector<std::string>& extra_includes) {
    std::ostringstream source;

    // Include the header for each function
    std::vector<std::string> included_headers;
    for (const auto& func : functions) {
        if (!func.header.empty()) {
            if (std::find(included_headers.begin(), included_headers.end(),
                          func.header) == included_headers.end()) {
                source << "#include \"" << func.header << "\"\n";
                included_headers.push_back(func.header);
            }
        }
    }

    for (const auto& inc : extra_includes) {
        source << "#include " << inc << "\n";
    }

    source << "\n";
    source << generate_stubs(functions, module_name);

    return source.str();
}

bool CppStubGen::write_stub_file(const std::string& path,
                                  const std::vector<CFunction>& functions,
                                  const std::string& module_name) {
    std::string source = generate_source(functions, module_name);
    std::ofstream file(path);
    if (!file.is_open()) {
        std::cerr << "Error: Could not write stub file to " << path << std::endl;
        return false;
    }
    file << source;
    file.close();
    return true;
}

} // namespace aot
} // namespace neutron