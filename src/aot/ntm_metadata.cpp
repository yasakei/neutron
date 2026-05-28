#include "aot/ntm_metadata.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>

namespace neutron {
namespace aot {

// ============== Name Mangling ==============

std::string NtmMetadata::mangle(const std::string& module_name, const std::string& symbol) {
    return module_name + "$" + symbol;
}

bool NtmMetadata::demangle(const std::string& mangled, std::string& module_name, std::string& symbol) {
    auto pos = mangled.find('$');
    if (pos == std::string::npos) return false;
    module_name = mangled.substr(0, pos);
    symbol = mangled.substr(pos + 1);
    return true;
}

// ============== Type Mapping ==============

NtmType NtmMetadata::value_type_to_ntm(ValueType vt) {
    switch (vt) {
        case ValueType::NIL:       return NtmType::NIL;
        case ValueType::BOOLEAN:   return NtmType::BOOL;
        case ValueType::NUMBER:    return NtmType::NUMBER;
        case ValueType::OBJ_STRING: return NtmType::STRING;
        case ValueType::ARRAY:     return NtmType::ARRAY;
        case ValueType::OBJECT:    return NtmType::OBJECT;
        case ValueType::CLASS:     return NtmType::CLASS;
        case ValueType::INSTANCE:  return NtmType::INSTANCE;
        case ValueType::CALLABLE:  return NtmType::CALLABLE;
        default:                   return NtmType::ANY;
    }
}

std::string NtmMetadata::ntm_to_qbe_type(NtmType t) {
    switch (t) {
        case NtmType::NIL:
        case NtmType::BOOL:    return "w";
        case NtmType::NUMBER:  return "d";
        case NtmType::STRING:
        case NtmType::ARRAY:
        case NtmType::OBJECT:
        case NtmType::CLASS:
        case NtmType::INSTANCE:
        case NtmType::CALLABLE:
        case NtmType::ANY:     return "l";
        default:               return "l";
    }
}

// ============== String Escaping ==============

std::string NtmMetadata::escape(const std::string& s) {
    std::string result;
    for (char c : s) {
        switch (c) {
            case '\\': result += "\\\\"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            case '"':  result += "\\\""; break;
            default:   result += c;
        }
    }
    return result;
}

std::string NtmMetadata::unescape(const std::string& s) {
    std::string result;
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            switch (s[i + 1]) {
                case '\\': result += '\\'; break;
                case 'n':  result += '\n'; break;
                case 'r':  result += '\r'; break;
                case 't':  result += '\t'; break;
                case '"':  result += '"'; break;
                default:   result += s[i + 1]; break;
            }
            i++;
        } else {
            result += s[i];
        }
    }
    return result;
}

// ============== Serialization ==============

std::string NtmMetadata::serialize(const NtmModule& module) {
    std::ostringstream out;

    out << "module \"" << escape(module.module_name) << "\"\n";

    for (const auto& func : module.functions) {
        out << "  export fun " << func.name;
        out << "(";
        for (size_t i = 0; i < func.params.size(); i++) {
            if (i > 0) out << ", ";
            out << func.params[i].name << ": ";
            switch (func.params[i].type) {
                case NtmType::NIL:     out << "nil"; break;
                case NtmType::BOOL:    out << "bool"; break;
                case NtmType::NUMBER:  out << "num"; break;
                case NtmType::STRING:  out << "str"; break;
                case NtmType::ARRAY:   out << "arr"; break;
                case NtmType::OBJECT:  out << "obj"; break;
                case NtmType::CLASS:   out << "class"; break;
                case NtmType::INSTANCE: out << "inst"; break;
                case NtmType::CALLABLE: out << "fn"; break;
                default:               out << "any"; break;
            }
        }
        out << ")";
        switch (func.return_type) {
            case NtmType::NIL:     out << " -> nil"; break;
            case NtmType::BOOL:    out << " -> bool"; break;
            case NtmType::NUMBER:  out << " -> num"; break;
            case NtmType::STRING:  out << " -> str"; break;
            case NtmType::ARRAY:   out << " -> arr"; break;
            case NtmType::OBJECT:  out << " -> obj"; break;
            case NtmType::CLASS:   out << " -> class"; break;
            case NtmType::INSTANCE: out << " -> inst"; break;
            case NtmType::CALLABLE: out << " -> fn"; break;
            default:               break;
        }
        out << "\n";
    }

    for (const auto& c : module.constants) {
        out << "  export const " << c.name;
        switch (c.type) {
            case NtmType::NIL:    out << " = nil"; break;
            case NtmType::BOOL:   out << " = " << c.value_str; break;
            case NtmType::NUMBER: out << " = " << c.value_str; break;
            case NtmType::STRING: out << " = \"" << escape(c.value_str) << "\""; break;
            default:              out << " = " << c.value_str; break;
        }
        out << "\n";
    }

    return out.str();
}

NtmModule NtmMetadata::deserialize(const std::string& data) {
    NtmModule module;
    std::istringstream in(data);
    std::string line;

    while (std::getline(in, line)) {
        // Trim whitespace
        auto not_space = [](char c) { return !std::isspace(static_cast<unsigned char>(c)); };
        auto first = std::find_if(line.begin(), line.end(), not_space);
        line.erase(line.begin(), first);
        auto last = std::find_if(line.rbegin(), line.rend(), not_space);
        line.erase(last.base(), line.end());

        if (line.empty()) continue;

        if (line.find("module ") == 0) {
            // module "name"
            auto q1 = line.find('"');
            auto q2 = line.rfind('"');
            if (q1 != std::string::npos && q2 != std::string::npos && q2 > q1) {
                module.module_name = unescape(line.substr(q1 + 1, q2 - q1 - 1));
            }
        } else if (line.find("export fun ") == 0) {
            // export fun name(param1: type1, ...) -> ret_type
            NtmExportFunc func;
            std::string rest = line.substr(11); // after "export fun "

            auto paren_open = rest.find('(');
            auto paren_close = rest.find(')');
            if (paren_open != std::string::npos) {
                func.name = rest.substr(0, paren_open);
                // Trim whitespace from name
                auto nf = std::find_if(func.name.begin(), func.name.end(), not_space);
                auto nl = std::find_if(func.name.rbegin(), func.name.rend(), not_space);
                if (nf < nl.base()) func.name = std::string(nf, nl.base());

                // Parse params
                std::string params_str = rest.substr(paren_open + 1, paren_close - paren_open - 1);
                if (!params_str.empty()) {
                    std::istringstream ps(params_str);
                    std::string param;
                    while (std::getline(ps, param, ',')) {
                        auto colon = param.find(':');
                        if (colon != std::string::npos) {
                            NtmParam p;
                            // Trim whitespace
                            auto pf = std::find_if(param.begin(), param.begin() + colon, not_space);
                            auto pl = std::find_if(param.rbegin(), param.rend(), not_space);
                            if (pf < pl.base()) {
                                // For the name, just trim end
                                std::string name_part(param.begin(), param.begin() + colon);
                                auto n_end = std::find_if(name_part.rbegin(), name_part.rend(), not_space);
                                p.name = std::string(name_part.begin(), n_end.base());
                            }
                            std::string type_str = param.substr(colon + 1);
                            auto tf = std::find_if(type_str.begin(), type_str.end(), not_space);
                            auto tl = std::find_if(type_str.rbegin(), type_str.rend(), not_space);
                            if (tf < tl.base()) type_str = std::string(tf, tl.base());

                            if (type_str == "nil") p.type = NtmType::NIL;
                            else if (type_str == "bool") p.type = NtmType::BOOL;
                            else if (type_str == "num") p.type = NtmType::NUMBER;
                            else if (type_str == "str") p.type = NtmType::STRING;
                            else if (type_str == "arr") p.type = NtmType::ARRAY;
                            else if (type_str == "obj") p.type = NtmType::OBJECT;
                            else if (type_str == "class") p.type = NtmType::CLASS;
                            else if (type_str == "inst") p.type = NtmType::INSTANCE;
                            else if (type_str == "fn") p.type = NtmType::CALLABLE;
                            else p.type = NtmType::ANY;
                            func.params.push_back(p);
                        }
                    }
                }
            }

            // Parse return type
            auto arrow = rest.find("->");
            if (arrow != std::string::npos) {
                std::string ret = rest.substr(arrow + 2);
                auto rf = std::find_if(ret.begin(), ret.end(), not_space);
                auto rl = std::find_if(ret.rbegin(), ret.rend(), not_space);
                if (rf < rl.base()) ret = std::string(rf, rl.base());
                if (ret == "nil") func.return_type = NtmType::NIL;
                else if (ret == "bool") func.return_type = NtmType::BOOL;
                else if (ret == "num") func.return_type = NtmType::NUMBER;
                else if (ret == "str") func.return_type = NtmType::STRING;
                else if (ret == "arr") func.return_type = NtmType::ARRAY;
                else if (ret == "obj") func.return_type = NtmType::OBJECT;
                else if (ret == "class") func.return_type = NtmType::CLASS;
                else if (ret == "inst") func.return_type = NtmType::INSTANCE;
                else if (ret == "fn") func.return_type = NtmType::CALLABLE;
                else func.return_type = NtmType::ANY;
            }

            func.mangled_name = mangle(module.module_name, func.name);
            module.functions.push_back(func);
        } else if (line.find("export const ") == 0) {
            NtmExportConst ec;
            std::string rest = line.substr(13); // after "export const "

            auto eq = rest.find('=');
            if (eq != std::string::npos) {
                std::string name_part = rest.substr(0, eq);
                auto nf = std::find_if(name_part.begin(), name_part.end(), not_space);
                auto nl = std::find_if(name_part.rbegin(), name_part.rend(), not_space);
                if (nf < nl.base()) ec.name = std::string(nf, nl.base());

                std::string val_part = rest.substr(eq + 1);
                auto vf = std::find_if(val_part.begin(), val_part.end(), not_space);
                auto vl = std::find_if(val_part.rbegin(), val_part.rend(), not_space);
                if (vf < vl.base()) ec.value_str = std::string(vf, vl.base());
            }

            // Determine type from value representation
            if (ec.value_str == "nil") {
                ec.type = NtmType::NIL;
            } else if (ec.value_str == "true" || ec.value_str == "false") {
                ec.type = NtmType::BOOL;
            } else if (ec.value_str.find('"') == 0) {
                ec.type = NtmType::STRING;
                // Remove quotes
                ec.value_str = ec.value_str.substr(1, ec.value_str.size() - 2);
                ec.value_str = unescape(ec.value_str);
            } else {
                ec.type = NtmType::NUMBER;
            }

            ec.mangled_name = mangle(module.module_name, ec.name);
            module.constants.push_back(ec);
        }
    }

    return module;
}

// ============== File I/O ==============

NtmModule NtmMetadata::read_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return NtmModule{};
    }
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();
    return deserialize(content);
}

bool NtmMetadata::write_file(const std::string& path, const NtmModule& module) {
    std::ofstream file(path);
    if (!file.is_open()) {
        std::cerr << "Error: Could not write .ntm file to " << path << std::endl;
        return false;
    }
    file << serialize(module);
    file.close();
    return true;
}

} // namespace aot
} // namespace neutron