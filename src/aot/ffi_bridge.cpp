#include "aot/ffi_bridge.h"

// Optional libclang support
#ifdef NEUTRON_HAVE_LIBCLANG
#include <clang-c/Index.h>
#endif

#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <cctype>

namespace neutron {
namespace aot {

std::string FFIBridge::trim(const std::string& s) {
    auto not_space = [](char c) { return !std::isspace(static_cast<unsigned char>(c)); };
    auto first = std::find_if(s.begin(), s.end(), not_space);
    auto last = std::find_if(s.rbegin(), s.rend(), not_space);
    if (first < last.base()) {
        return std::string(first, last.base());
    }
    return s;
}

std::vector<std::string> FFIBridge::split(const std::string& s, char delim) {
    std::vector<std::string> parts;
    std::istringstream stream(s);
    std::string part;
    while (std::getline(stream, part, delim)) {
        parts.push_back(trim(part));
    }
    return parts;
}

CType FFIBridge::type_from_string(const std::string& type_str) {
    std::string t = trim(type_str);

    // Handle pointers
    if (t.find('*') != std::string::npos) {
        // Check for char* / const char* -> string
        std::string base = t;
        base.erase(std::remove(base.begin(), base.end(), '*'), base.end());
        base.erase(std::remove(base.begin(), base.end(), ' '), base.end());
        base.erase(std::remove(base.begin(), base.end(), 'c'), base.end());
        base.erase(std::remove(base.begin(), base.end(), 'o'), base.end());
        base.erase(std::remove(base.begin(), base.end(), 'n'), base.end());
        base.erase(std::remove(base.begin(), base.end(), 's'), base.end());
        base.erase(std::remove(base.begin(), base.end(), 't'), base.end());
        if (base == "char") return CType::STRING;
        return CType::POINTER;
    }

    if (t == "void") return CType::VOID;
    if (t == "bool" || t == "_Bool") return CType::BOOL;
    if (t == "int8_t" || t == "int8" || t == "signed char") return CType::INT8;
    if (t == "int16_t" || t == "int16" || t == "short") return CType::INT16;
    if (t == "int32_t" || t == "int32" || t == "int" || t == "signed int") return CType::INT32;
    if (t == "int64_t" || t == "int64" || t == "long long" || t == "int64_t") return CType::INT64;
    if (t == "uint8_t" || t == "uint8" || t == "unsigned char") return CType::UINT8;
    if (t == "uint16_t" || t == "uint16" || t == "unsigned short") return CType::UINT16;
    if (t == "uint32_t" || t == "uint32" || t == "unsigned int" || t == "unsigned") return CType::UINT32;
    if (t == "uint64_t" || t == "uint64" || t == "unsigned long long") return CType::UINT64;
    if (t == "float") return CType::FLOAT;
    if (t == "double") return CType::DOUBLE;
    if (t.find("char") != std::string::npos) return CType::STRING;

    return CType::UNKNOWN;
}

std::string FFIBridge::type_to_qbe(CType t) {
    switch (t) {
        case CType::VOID:   return "";
        case CType::BOOL:
        case CType::INT8:
        case CType::INT16:
        case CType::INT32:
        case CType::UINT8:
        case CType::UINT16:
        case CType::UINT32: return "w";
        case CType::INT64:
        case CType::UINT64:
        case CType::POINTER:
        case CType::STRING: return "l";
        case CType::FLOAT:  return "s";
        case CType::DOUBLE: return "d";
        default:            return "l";
    }
}

int FFIBridge::type_to_tag(CType t) {
    switch (t) {
        case CType::VOID:   return 1; // NIL
        case CType::BOOL:   return 2; // BOOL
        case CType::INT8:
        case CType::INT16:
        case CType::INT32:
        case CType::INT64:
        case CType::UINT8:
        case CType::UINT16:
        case CType::UINT32:
        case CType::UINT64:
        case CType::FLOAT:
        case CType::DOUBLE: return 3; // NUMBER
        case CType::POINTER:
        case CType::STRING: return 4; // STRING/pointer
        default:            return 1;
    }
}

bool FFIBridge::has_libclang() {
#ifdef NEUTRON_HAVE_LIBCLANG
    return true;
#else
    return false;
#endif
}

// ============== Manual Declaration Parser ==============

std::vector<CFunction> FFIBridge::parse_declarations(const std::string& source) {
    std::vector<CFunction> functions;
    std::istringstream stream(source);
    std::string line;

    while (std::getline(stream, line)) {
        line = trim(line);
        if (line.empty() || line.find("//") == 0 || line.find("#") == 0) continue;

        // Look for function declarations ending with ;
        // Pattern: [return_type] [name]([params]);
        auto paren_open = line.find('(');
        auto paren_close = line.find(')');
        if (paren_open == std::string::npos || paren_close == std::string::npos) continue;
        if (paren_close < paren_open) continue;

        CFunction func;
        func.is_extern_c = false;

        // Check for extern "C"
        auto extern_pos = line.find("extern \"C\"");
        if (extern_pos != std::string::npos) {
            func.is_extern_c = true;
            // Remove extern "C" from the line for easier parsing
            line.erase(extern_pos, 9); // "extern \"C\"".length()
            line = trim(line);
            // Recalculate positions
            paren_open = line.find('(');
            paren_close = line.find(')');
            if (paren_open == std::string::npos) continue;
        }

        // Extract return type and function name from the part before (
        std::string before_paren = trim(line.substr(0, paren_open));
        auto last_space = before_paren.rfind(' ');
        if (last_space == std::string::npos) continue;

        func.name = before_paren.substr(last_space + 1);
        std::string ret_type_str = trim(before_paren.substr(0, last_space));

        // Handle pointer return types
        if (func.name.find('*') != std::string::npos) {
            // e.g., "char* getName()" -> name is "getName", return is "char*"
            // Find the last non-pointer word
            size_t ptr_pos = func.name.find('*');
            if (ptr_pos == 0) {
                // Pointer is part of return type: "int* getName"
                ret_type_str = trim(before_paren.substr(0, last_space)) + "*";
                func.name = before_paren.substr(last_space + 1);
                // Remove leading *
                while (!func.name.empty() && func.name[0] == '*') {
                    ret_type_str += "*";
                    func.name = trim(func.name.substr(1));
                }
            } else {
                // Pointer is part of name: "int *getName" -> not standard
                ret_type_str = trim(before_paren.substr(0, last_space));
                func.name = before_paren.substr(last_space + 1);
            }
        }

        func.return_type = type_from_string(ret_type_str);

        // Extract parameters
        std::string params_str = line.substr(paren_open + 1, paren_close - paren_open - 1);
        if (!params_str.empty() && params_str != "void") {
            std::vector<std::string> params = split(params_str, ',');
            for (const auto& p : params) {
                std::string trimmed = trim(p);
                if (trimmed.empty()) continue;

                // Split into type and name
                std::string ptype, pname;
                auto space_pos = trimmed.rfind(' ');
                if (space_pos != std::string::npos) {
                    pname = trimmed.substr(space_pos + 1);
                    ptype = trimmed.substr(0, space_pos);
                } else {
                    ptype = trimmed;
                }

                // Handle "void" parameter means no parameters
                if (ptype == "void" && pname.empty()) break;

                func.param_types.push_back(type_from_string(ptype));
                func.param_names.push_back(pname);
            }
        }

        functions.push_back(func);
    }

    return functions;
}

// ============== Header Scanning ==============

CHeaderScanResult FFIBridge::scan_header(const std::string& header_path,
                                          const std::vector<std::string>& include_dirs) {
    CHeaderScanResult result;
    result.header_path = header_path;
    result.success = false;

    // Read the header file
    std::ifstream file(header_path);
    if (!file.is_open()) {
        result.error_msg = "Could not open header: " + header_path;
        return result;
    }

    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();

#ifdef NEUTRON_HAVE_LIBCLANG
    // Use libclang for accurate parsing
    CXIndex index = clang_createIndex(0, 0);
    if (!index) {
        result.error_msg = "Failed to create libclang index";
        return result;
    }

    std::vector<const char*> clang_args;
    clang_args.push_back("-x");
    clang_args.push_back("c++");
    for (const auto& dir : include_dirs) {
        clang_args.push_back(("-I" + dir).c_str());
    }

    CXTranslationUnit tu = clang_parseTranslationUnit(index, header_path.c_str(),
        clang_args.data(), clang_args.size(), nullptr, 0,
        CXTranslationUnit_None);

    if (!tu) {
        result.error_msg = "Failed to parse translation unit";
        clang_disposeIndex(index);
        return result;
    }

    // Get the cursor for the translation unit
    CXCursor cursor = clang_getTranslationUnitCursor(tu);

    // Visit all children to find function declarations
    // We wrap this in a helper struct to capture results
    struct VisitData {
        std::vector<CFunction>* funcs;
        std::string header;
    };
    VisitData visit_data{&result.functions, header_path};

    clang_visitChildren(cursor,
        [](CXCursor c, CXCursor parent, CXClientData client_data) {
            auto* data = static_cast<VisitData*>(client_data);

            if (clang_getCursorKind(c) == CXCursor_FunctionDecl) {
                // Check for extern "C" linkage
                CXLinkageKind linkage = clang_getCursorLinkage(c);
                bool is_extern_c = (linkage == CXLinkage_External);

                CXString name_cx = clang_getCursorSpelling(c);
                std::string name = clang_getCString(name_cx);
                clang_disposeString(name_cx);

                CFunction func;
                func.name = name;
                func.is_extern_c = is_extern_c;
                func.header = data->header;

                // Return type
                CXType ret_type = clang_getResultType(clang_getCursorType(c));
                CXString ret_spelling = clang_getTypeSpelling(ret_type);
                func.return_type = FFIBridge::type_from_string(clang_getCString(ret_spelling));
                clang_disposeString(ret_spelling);

                // Parameters
                int num_args = clang_Cursor_getNumArguments(c);
                for (int i = 0; i < num_args; i++) {
                    CXCursor arg = clang_getArgument(c, i);
                    CXType arg_type = clang_getCursorType(arg);
                    CXString arg_name_cx = clang_getCursorSpelling(arg);
                    CXString arg_type_cx = clang_getTypeSpelling(arg_type);

                    func.param_types.push_back(
                        FFIBridge::type_from_string(clang_getCString(arg_type_cx)));
                    func.param_names.push_back(clang_getCString(arg_name_cx));

                    clang_disposeString(arg_type_cx);
                    clang_disposeString(arg_name_cx);
                }

                data->funcs->push_back(func);
            }
            return CXChildVisit_Continue;
        },
        &visit_data);

    clang_disposeTranslationUnit(tu);
    clang_disposeIndex(index);

    result.success = true;
#else
    // Fall back to manual parsing
    result.functions = parse_declarations(content);
    result.success = true;
#endif

    return result;
}

} // namespace aot
} // namespace neutron