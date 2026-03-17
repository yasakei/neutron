/*
 * Neutron Programming Language - AOT Compiler Implementation
 * Copyright (c) 2026 yasakei
 */

#include "aot/aot_compiler.h"
#include "types/obj_string.h"
#include "types/function.h"
#include <iostream>
#include <iomanip>
#include <cmath>
#include <unordered_map>

namespace neutron {
namespace aot {

AotCompiler::AotCompiler(const Chunk* c) : chunk(c), ip(0), generateDebugSymbols(false), currentCppLine(0) {}

// First pass: collect all global variable definitions
void AotCompiler::collectGlobalDefinitions() {
    globalNames.clear();
    globalSlotMap.clear();
    
    size_t scanIp = 0;
    int globalSlot = 0;
    
    while (scanIp < chunk->code.size()) {
        uint8_t instruction = chunk->code[scanIp++];
        OpCode op = static_cast<OpCode>(instruction);
        
        if (op == OpCode::OP_DEFINE_GLOBAL || op == OpCode::OP_DEFINE_TYPED_GLOBAL) {
            uint8_t nameIndex = chunk->code[scanIp++];  // Compiler uses 1-byte when possible

            if (nameIndex < chunk->constants.size()) {
                Value nameValue = chunk->constants[nameIndex];
                if (nameValue.type == ValueType::OBJ_STRING) {
                    std::string name = nameValue.as.obj_string->chars;
                    globalNames.push_back(name);
                    globalSlotMap[name] = globalSlot++;
                }
            }
        } else if (op == OpCode::OP_CONSTANT || op == OpCode::OP_GET_LOCAL ||
                   op == OpCode::OP_SET_LOCAL || op == OpCode::OP_GET_UPVALUE ||
                   op == OpCode::OP_SET_UPVALUE || op == OpCode::OP_CLOSE_UPVALUE ||
                   op == OpCode::OP_GET_PROPERTY || op == OpCode::OP_SET_PROPERTY ||
                   op == OpCode::OP_GET_GLOBAL || op == OpCode::OP_SET_GLOBAL ||
                   op == OpCode::OP_SET_GLOBAL_TYPED || op == OpCode::OP_SET_LOCAL_TYPED ||
                   op == OpCode::OP_GET_GLOBAL_FAST || op == OpCode::OP_SET_GLOBAL_FAST) {
            scanIp++;  // Skip 1-byte operand
        } else if (op == OpCode::OP_CONSTANT_LONG) {
            scanIp += 2;  // Skip 2-byte operand
        } else if (op == OpCode::OP_CALL || op == OpCode::OP_CALL_FAST ||
                   op == OpCode::OP_ARRAY || op == OpCode::OP_INVOKE) {
            scanIp++;  // Skip arg count
        } else if (op == OpCode::OP_INCREMENT_LOCAL || op == OpCode::OP_DECREMENT_LOCAL ||
                   op == OpCode::OP_INC_LOCAL_INT || op == OpCode::OP_DEC_LOCAL_INT ||
                   op == OpCode::OP_CONST_INT8) {
            scanIp++;  // Skip 1-byte operand
        } else if (op == OpCode::OP_ADD_LOCAL_CONST) {
            scanIp += 2;  // Skip slot + const index
        } else if (op == OpCode::OP_INCREMENT_GLOBAL) {
            scanIp += 2;  // Skip global name index
        } else if (op == OpCode::OP_JUMP || op == OpCode::OP_JUMP_IF_FALSE ||
                   op == OpCode::OP_LOOP || op == OpCode::OP_LESS_JUMP ||
                   op == OpCode::OP_GREATER_JUMP || op == OpCode::OP_EQUAL_JUMP) {
            scanIp += 2;  // Skip 2-byte offset
        } else if (op == OpCode::OP_LOOP_IF_LESS_LOCAL) {
            scanIp += 3;  // Skip slot (1 byte) + offset (2 bytes)
        } else if (op == OpCode::OP_CLOSURE) {
            scanIp += 2;  // Skip function index
            if (scanIp < chunk->code.size()) {
                uint16_t numUpvalues = (chunk->code[scanIp] << 8) | chunk->code[scanIp + 1];
                scanIp += 2 + numUpvalues * 2;  // Skip upvalue info
            }
        }
        // Other opcodes have no operands (pure stack operations, arithmetic, comparisons, etc.)
    }
}

// Optimization pass: constant folding and simple dead code elimination
void AotCompiler::optimizeBytecode(std::vector<uint8_t>& code, std::vector<Value>& constants) {
    if (code.empty()) return;
    
    bool changed = true;
    int iterations = 0;
    const int MAX_ITERATIONS = 10;  // Prevent infinite loops

    while (changed && iterations < MAX_ITERATIONS) {
        changed = false;
        iterations++;

        size_t ip = 0;
        std::unordered_map<int, double> knownConstants;  // local slot -> known value
        std::unordered_map<int, ValueType> knownTypes;   // local slot -> known type

        // Phase 1: Constant Propagation and Folding
        while (ip < code.size()) {
            size_t instrStart = ip;
            uint8_t instruction = code[ip++];
            OpCode op = static_cast<OpCode>(instruction);

            // Helper to read operands
            auto readOperand = [&]() -> uint8_t {
                if (ip < code.size()) return code[ip++];
                return 0;
            };
            auto readShort = [&]() -> uint16_t {
                if (ip + 1 < code.size()) {
                    uint16_t val = (code[ip] << 8) | code[ip + 1];
                    ip += 2;
                    return val;
                }
                return 0;
            };

            // Skip operands for all opcodes that have them
            switch (op) {
                case OpCode::OP_CONSTANT:
                case OpCode::OP_GET_LOCAL:
                case OpCode::OP_SET_LOCAL:
                case OpCode::OP_GET_UPVALUE:
                case OpCode::OP_SET_UPVALUE:
                case OpCode::OP_CLOSE_UPVALUE:
                case OpCode::OP_GET_PROPERTY:
                case OpCode::OP_SET_PROPERTY:
                case OpCode::OP_GET_GLOBAL:
                case OpCode::OP_SET_GLOBAL:
                case OpCode::OP_SET_GLOBAL_TYPED:
                case OpCode::OP_SET_LOCAL_TYPED:
                case OpCode::OP_GET_GLOBAL_FAST:
                case OpCode::OP_SET_GLOBAL_FAST:
                case OpCode::OP_DEFINE_GLOBAL:
                case OpCode::OP_DEFINE_TYPED_GLOBAL:
                case OpCode::OP_INCREMENT_LOCAL:
                case OpCode::OP_DECREMENT_LOCAL:
                case OpCode::OP_INC_LOCAL_INT:
                case OpCode::OP_DEC_LOCAL_INT:
                case OpCode::OP_CONST_INT8:
                case OpCode::OP_CALL:
                case OpCode::OP_CALL_FAST:
                case OpCode::OP_ARRAY:
                case OpCode::OP_INVOKE:
                    if (ip < code.size()) ip++;
                    break;

                case OpCode::OP_CONSTANT_LONG:
                    if (ip + 1 < code.size()) ip += 2;
                    break;

                case OpCode::OP_ADD_LOCAL_CONST:
                    if (ip + 1 < code.size()) ip += 2;
                    break;

                case OpCode::OP_INCREMENT_GLOBAL:
                    if (ip + 1 < code.size()) ip += 2;
                    break;

                case OpCode::OP_JUMP:
                case OpCode::OP_JUMP_IF_FALSE:
                case OpCode::OP_LOOP:
                case OpCode::OP_LESS_JUMP:
                case OpCode::OP_GREATER_JUMP:
                case OpCode::OP_EQUAL_JUMP:
                    if (ip + 1 < code.size()) ip += 2;
                    break;

                case OpCode::OP_LOOP_IF_LESS_LOCAL:
                    if (ip + 2 < code.size()) ip += 3;
                    break;

                case OpCode::OP_CLOSURE:
                    if (ip + 1 < code.size()) {
                        ip++;  // function index
                        if (ip + 1 < code.size()) {
                            uint16_t numUpvalues = (code[ip] << 8) | code[ip + 1];
                            if (ip + 2 + numUpvalues * 2 <= code.size()) {
                                ip += 2 + numUpvalues * 2;
                            }
                        }
                    }
                    break;

                default:
                    // No operands
                    break;
            }

            size_t instrEnd = ip;

            // Track constant loads into locals
            if (op == OpCode::OP_CONST_ZERO) {
                // Next instruction should be SET_LOCAL for propagation
                if (ip < code.size() && code[ip] == static_cast<uint8_t>(OpCode::OP_SET_LOCAL)) {
                    uint8_t slot = code[ip + 1];
                    knownConstants[slot] = 0.0;
                    knownTypes[slot] = ValueType::NUMBER;
                    changed = true;
                }
            } else if (op == OpCode::OP_CONST_ONE) {
                if (ip < code.size() && code[ip] == static_cast<uint8_t>(OpCode::OP_SET_LOCAL)) {
                    uint8_t slot = code[ip + 1];
                    knownConstants[slot] = 1.0;
                    knownTypes[slot] = ValueType::NUMBER;
                    changed = true;
                }
            } else if (op == OpCode::OP_CONSTANT) {
                uint8_t constIndex = code[instrStart + 1];
                if (constIndex < constants.size() && constants[constIndex].type == ValueType::NUMBER) {
                    // Check if followed by SET_LOCAL
                    if (ip < code.size() && code[ip] == static_cast<uint8_t>(OpCode::OP_SET_LOCAL)) {
                        uint8_t slot = code[ip + 1];
                        knownConstants[slot] = constants[constIndex].as.number;
                        knownTypes[slot] = ValueType::NUMBER;
                        changed = true;
                    }
                }
            } else if (op == OpCode::OP_SET_LOCAL) {
                uint8_t slot = code[instrStart + 1];
                knownTypes[slot] = ValueType::NIL;  // Unknown value written
                knownConstants.erase(slot);
            } else if (op == OpCode::OP_SET_LOCAL_TYPED) {
                uint8_t slot = code[instrStart + 1];
                knownTypes[slot] = ValueType::NIL;
                knownConstants.erase(slot);
            }

            // Fold INCREMENT_LOCAL when we know the current value
            if (op == OpCode::OP_INCREMENT_LOCAL && ip > 1) {
                uint8_t slot = code[instrStart + 1];
                auto it = knownConstants.find(slot);
                if (it != knownConstants.end()) {
                    knownConstants[slot] = it->second + 1.0;
                    changed = true;
                    // TODO: Replace with constant load + set for better optimization
                }
            }

            // Fold DECREMENT_LOCAL
            if (op == OpCode::OP_DECREMENT_LOCAL && ip > 1) {
                uint8_t slot = code[instrStart + 1];
                auto it = knownConstants.find(slot);
                if (it != knownConstants.end()) {
                    knownConstants[slot] = it->second - 1.0;
                    changed = true;
                }
            }

            // Fold arithmetic operations on known constants (stack-based)
            // Pattern: CONST a, CONST b, OP_ADD -> CONST (a+b)
            // This requires more complex bytecode rewriting, deferred for now
        }
    }

    // Phase 2: Dead Code Elimination - Remove unreachable code after unconditional jumps
    // This is a simple implementation that can be enhanced later
    std::vector<uint8_t> optimized;
    optimized.reserve(code.size());
    
    std::vector<size_t> jumpTargets;
    std::vector<size_t> reachableInstructions;
    
    // First pass: identify all jump targets
    size_t ip = 0;
    while (ip < code.size()) {
        uint8_t instruction = code[ip++];
        OpCode op = static_cast<OpCode>(instruction);
        
        auto readShort = [&]() -> uint16_t {
            if (ip + 1 < code.size()) {
                uint16_t val = (code[ip] << 8) | code[ip + 1];
                ip += 2;
                return val;
            }
            ip += 2;
            return 0;
        };
        
        switch (op) {
            case OpCode::OP_JUMP:
            case OpCode::OP_JUMP_IF_FALSE:
            case OpCode::OP_LOOP:
            case OpCode::OP_LESS_JUMP:
            case OpCode::OP_GREATER_JUMP:
            case OpCode::OP_EQUAL_JUMP:
                jumpTargets.push_back(ip + readShort());
                break;
                
            case OpCode::OP_RETURN:
                // Instructions after RETURN until next label are dead
                break;
                
            default:
                // Skip operands
                if (op == OpCode::OP_CONSTANT || op == OpCode::OP_GET_LOCAL || 
                    op == OpCode::OP_SET_LOCAL) {
                    ip++;
                }
                break;
        }
    }
    
    // For now, just copy the code - full DCE requires more complex analysis
    // The constant propagation above already provides significant optimization
}

// Debug helper: emit a comment with source information
void AotCompiler::emitDebugComment(const std::string& comment) {
    if (generateDebugSymbols) {
        code << "    // " << comment << "\n";
    }
}

// Debug helper: record source location for mapping
void AotCompiler::recordSourceLocation(size_t bytecodeOffset) {
    if (generateDebugSymbols) {
        // Count current line
        std::string currentOutput = code.str();
        size_t lineCount = 1;
        for (char c : currentOutput) {
            if (c == '\n') lineCount++;
        }
        sourceMap[bytecodeOffset] = lineCount;
        currentCppLine = lineCount;
    }
}

uint8_t AotCompiler::readByte() {
    return chunk->code[ip++];
}

uint16_t AotCompiler::readShort() {
    uint8_t b1 = readByte();
    uint8_t b2 = readByte();
    return (static_cast<uint16_t>(b1) << 8) | b2;
}

std::string AotCompiler::constantToCpp(size_t index) {
    if (index >= chunk->constants.size()) {
        return "Value()";
    }
    
    const Value& v = chunk->constants[index];
    switch (v.type) {
        case ValueType::NIL:
            return "Value()";
        case ValueType::BOOLEAN:
            return v.as.boolean ? "Value(true)" : "Value(false)";
        case ValueType::NUMBER: {
            double num = v.as.number;
            // Check if it's an integer
            if (num == std::floor(num) && std::abs(num) < 1e15) {
                return "Value(" + std::to_string(static_cast<long long>(num)) + ".0)";
            }
            return "Value(" + std::to_string(num) + ")";
        }
        case ValueType::OBJ_STRING: {
            std::string str = v.as.obj_string->chars;
            std::string escaped;
            for (char c : str) {
                switch (c) {
                    case '"': escaped += "\\\""; break;
                    case '\\': escaped += "\\\\"; break;
                    case '\n': escaped += "\\n"; break;
                    case '\r': escaped += "\\r"; break;
                    case '\t': escaped += "\\t"; break;
                    default: escaped += c;
                }
            }
            return "Value(\"" + escaped + "\")";
        }
        default:
            return "Value()";
    }
}

void AotCompiler::generatePrologue(const std::string& functionName) {
    if (useSharedRuntime) {
        // Use the shared Neutron runtime header
        code << "#include \"types/value.h\"\n";
        code << "#include \"types/obj_string.h\"\n";
        code << "#include <iostream>\n";
        code << "#include <cmath>\n";
        code << "#include <string>\n";
        code << "#include <cstdint>\n\n";
        
        if (generateDebugSymbols) {
            code << "// Debug mode enabled - source map generated\n";
            code << "// Bytecode offset -> C++ line mapping available via getSourceMap()\n\n";
        }
        
        code << "// AOT-compiled Neutron code (using shared runtime)\n";
    } else {
        // Self-contained mode - generate everything inline
        code << "#include <iostream>\n";
        code << "#include <cmath>\n";
        code << "#include <string>\n";
        code << "#include <cstdint>\n\n";

        if (generateDebugSymbols) {
            code << "// Debug mode enabled - source map generated\n";
            code << "// Bytecode offset -> C++ line mapping available via getSourceMap()\n\n";
        }

        code << "// AOT-compiled Neutron code (self-contained)\n";
        
        // Generate ValueType enum matching the runtime exactly
        code << "// Value types - must match types/value.h exactly\n";
        code << "enum class ValueType {\n";
        code << "    NIL,         ///< nil\n";
        code << "    BOOLEAN,     ///< bool\n";
        code << "    NUMBER,      ///< double\n";
        code << "    OBJ_STRING,  ///< string object\n";
        code << "    ARRAY,       ///< array object\n";
        code << "    OBJECT,      ///< generic object\n";
        code << "    CALLABLE,    ///< function\n";
        code << "    MODULE,      ///< module\n";
        code << "    CLASS,       ///< class\n";
        code << "    INSTANCE,    ///< class instance\n";
        code << "    BUFFER       ///< binary buffer\n";
        code << "};\n\n";

        // Generate ValueUnion matching the runtime exactly
        code << "// Forward declarations for object types\n";
        code << "struct ObjString { std::string chars; };\n";
        code << "struct Array { std::vector<Value> elements; };\n";
        code << "struct Object { std::unordered_map<std::string, Value> properties; };\n";
        code << "struct Callable { virtual void operator()() = 0; };\n";
        code << "struct Module { std::string name; };\n";
        code << "struct Class { std::string name; };\n";
        code << "struct Instance { std::unordered_map<std::string, Value> fields; };\n";
        code << "struct Buffer { std::vector<uint8_t> data; };\n\n";

        code << "// ValueUnion - tagged union matching types/value.h\n";
        code << "union ValueUnion {\n";
        code << "    bool boolean;\n";
        code << "    double number;\n";
        code << "    ObjString* obj_string;\n";
        code << "    Array* array;\n";
        code << "    Object* object;\n";
        code << "    Callable* callable;\n";
        code << "    Module* module;\n";
        code << "    Class* klass;\n";
        code << "    Instance* instance;\n";
        code << "    Buffer* buffer;\n";
        code << "    \n";
        code << "    ValueUnion() : number(0) {}\n";
        code << "};\n\n";

        // Generate Value struct matching the runtime exactly
        code << "// Value struct - must match types/value.h exactly\n";
        code << "struct Value {\n";
        code << "    ValueType type;\n";
        code << "    ValueUnion as;\n";
        code << "    \n";
        code << "    Value() : type(ValueType::NIL) {}\n";
        code << "    Value(bool b) : type(ValueType::BOOLEAN) { as.boolean = b; }\n";
        code << "    Value(double n) : type(ValueType::NUMBER) { as.number = n; }\n";
        code << "    Value(const char* s) : type(ValueType::OBJ_STRING) { as.obj_string = new ObjString{s}; }\n";
        code << "    \n";
        code << "    // Type-safe accessors\n";
        code << "    bool isNil() const { return type == ValueType::NIL; }\n";
        code << "    bool isBoolean() const { return type == ValueType::BOOLEAN; }\n";
        code << "    bool isNumber() const { return type == ValueType::NUMBER; }\n";
        code << "    bool isString() const { return type == ValueType::OBJ_STRING; }\n";
        code << "    \n";
        code << "    // Value conversions\n";
        code << "    bool asBool() const { return type == ValueType::BOOLEAN ? as.boolean : false; }\n";
        code << "    double asNumber() const { return type == ValueType::NUMBER ? as.number : 0.0; }\n";
        code << "    std::string asString() const { return type == ValueType::OBJ_STRING && as.obj_string ? as.obj_string->chars : \"\"; }\n";
        code << "    \n";
        code << "    // toString for debugging\n";
        code << "    std::string toString() const {\n";
        code << "        switch (type) {\n";
        code << "            case ValueType::NIL: return \"nil\";\n";
        code << "            case ValueType::BOOLEAN: return as.boolean ? \"true\" : \"false\";\n";
        code << "            case ValueType::NUMBER: {\n";
        code << "                double n = as.number;\n";
        code << "                if (n == static_cast<long long>(n)) return std::to_string(static_cast<long long>(n));\n";
        code << "                return std::to_string(n);\n";
        code << "            }\n";
        code << "            case ValueType::OBJ_STRING: return as.obj_string ? as.obj_string->chars : \"\";\n";
        code << "            default: return \"<object>\";\n";
        code << "        }\n";
        code << "    }\n";
        code << "};\n\n";
    }

    // Target platform info
    code << "// Target platform: ";
    switch (targetPlatform) {
        case TargetPlatform::LINUX_X64:
            code << "// Target: Linux x86_64\n";
            break;
        case TargetPlatform::LINUX_ARM64:
            code << "// Target: Linux ARM64\n";
            break;
        case TargetPlatform::MACOS_X64:
            code << "// Target: macOS x86_64\n";
            break;
        case TargetPlatform::MACOS_ARM64:
            code << "// Target: macOS ARM64 (Apple Silicon)\n";
            break;
        case TargetPlatform::WINDOWS_X64:
            code << "// Target: Windows x64\n";
            break;
        case TargetPlatform::WINDOWS_X86:
            code << "// Target: Windows x86\n";
            break;
        default:
            code << "// Target: Native (current platform)\n";
            break;
    }
    code << "\n";

    // Minimal Value struct for AOT execution
    code << "enum class ValueType { NIL, BOOLEAN, NUMBER, OBJ_STRING };\n\n";

    code << "struct Value {\n";
    code << "    ValueType type;\n";
    code << "    union { bool boolean; double number; const char* string; } as;\n";
    code << "    \n";
    code << "    Value() : type(ValueType::NIL) { as.number = 0; }\n";
    code << "    Value(bool b) : type(ValueType::BOOLEAN) { as.boolean = b; }\n";
    code << "    Value(double n) : type(ValueType::NUMBER) { as.number = n; }\n";
    code << "    Value(const char* s) : type(ValueType::OBJ_STRING) { as.string = s; }\n";
    code << "    \n";
    code << "    std::string toString() const {\n";
    code << "        switch (type) {\n";
    code << "            case ValueType::NIL: return \"nil\";\n";
    code << "            case ValueType::BOOLEAN: return as.boolean ? \"true\" : \"false\";\n";
    code << "            case ValueType::NUMBER: {\n";
    code << "                double n = as.number;\n";
    code << "                if (n == (long long)n) return std::to_string((long long)n);\n";
    code << "                return std::to_string(n);\n";
    code << "            }\n";
    code << "            case ValueType::OBJ_STRING: return std::string(as.string);\n";
    code << "            default: return \"unknown\";\n";
    code << "        }\n";
    code << "    }\n";
    code << "};\n\n";

    // Constants array
    code << "// Embedded constants\n";
    code << "static const Value constants[] = {\n";
    for (size_t i = 0; i < chunk->constants.size(); i++) {
        code << "    " << constantToCpp(i) << ",\n";
    }
    code << "};\n\n";

    // Global variables
    if (!globalNames.empty()) {
        code << "// Global variables\n";
        for (const auto& name : globalNames) {
            code << "static Value global_" << name << ";\n";
        }
        code << "\n";
    }

    // Static array pool for AOT-compiled arrays
    if (!staticArrays.empty()) {
        code << "// Static array pool\n";
        for (const auto& arr : staticArrays) {
            code << "static Value array_" << arr.first << "[" << arr.second << "];\n";
        }
        code << "\n";
    }

    // Main function
    code << "int " << functionName << "() {\n";
    code << "    // Stack and locals\n";
    code << "    Value stack[256];\n";
    code << "    Value locals[256];\n";
    code << "    int sp = 0;  // Stack pointer\n";
    code << "    (void)stack; (void)locals; (void)sp;  // Suppress unused warnings\n\n";
}

void AotCompiler::generateEpilogue() {
    code << "    return 0;\n";
    code << "}\n";
}

void AotCompiler::generateBytecodeBody() {
    ip = 0;
    size_t codeSize = chunk->code.size();

    // First pass: find jump targets
    std::vector<bool> isJumpTarget(codeSize + 1, false);
    size_t scanIp = 0;
    while (scanIp < codeSize) {
        size_t instrStart = scanIp;
        uint8_t instruction = chunk->code[scanIp++];
        OpCode op = static_cast<OpCode>(instruction);

        switch (op) {
            case OpCode::OP_JUMP:
            case OpCode::OP_JUMP_IF_FALSE:
            case OpCode::OP_LOOP:
            case OpCode::OP_LESS_JUMP:
            case OpCode::OP_GREATER_JUMP:
            case OpCode::OP_EQUAL_JUMP: {
                if (scanIp + 1 >= codeSize) break;
                uint16_t offset = (chunk->code[scanIp] << 8) | chunk->code[scanIp + 1];
                scanIp += 2;
                size_t target;
                if (op == OpCode::OP_LOOP) {
                    target = (scanIp > offset) ? (scanIp - offset) : 0;
                } else {
                    target = scanIp + offset;
                }
                if (target < codeSize) isJumpTarget[target] = true;
                break;
            }
            case OpCode::OP_LOOP_IF_LESS_LOCAL: {
                scanIp++; // slot
                scanIp++; // constant index
                uint16_t offset = (chunk->code[scanIp] << 8) | chunk->code[scanIp + 1];
                scanIp += 2;
                // Forward jump to exit loop (when condition is false)
                size_t target = scanIp + offset;
                if (target <= codeSize) isJumpTarget[target] = true;
                break;
            }
            default:
                // Skip operands for all opcodes that have them
                // 1-byte operands
                if (op == OpCode::OP_CONSTANT || op == OpCode::OP_GET_LOCAL ||
                    op == OpCode::OP_SET_LOCAL || op == OpCode::OP_GET_UPVALUE ||
                    op == OpCode::OP_SET_UPVALUE || op == OpCode::OP_CLOSE_UPVALUE ||
                    op == OpCode::OP_GET_PROPERTY || op == OpCode::OP_SET_PROPERTY ||
                    op == OpCode::OP_GET_GLOBAL_FAST || op == OpCode::OP_SET_GLOBAL_FAST ||
                    op == OpCode::OP_INCREMENT_LOCAL || op == OpCode::OP_DECREMENT_LOCAL ||
                    op == OpCode::OP_INC_LOCAL_INT || op == OpCode::OP_DEC_LOCAL_INT ||
                    op == OpCode::OP_CONST_INT8 || op == OpCode::OP_CALL ||
                    op == OpCode::OP_CALL_FAST || op == OpCode::OP_ARRAY ||
                    op == OpCode::OP_INVOKE) {
                    scanIp++;
                }
                // 2-byte operands (constant index or global name index)
                else if (op == OpCode::OP_CONSTANT_LONG || op == OpCode::OP_ADD_LOCAL_CONST) {
                    scanIp += 2;
                }
                // 1-byte operands for typed global/local ops
                else if (op == OpCode::OP_SET_GLOBAL_TYPED || op == OpCode::OP_SET_LOCAL_TYPED) {
                    scanIp++;
                }
                // 1-byte operands for global ops (compiler uses 1-byte when possible)
                else if (op == OpCode::OP_GET_GLOBAL || op == OpCode::OP_SET_GLOBAL || op == OpCode::OP_DEFINE_GLOBAL || op == OpCode::OP_INCREMENT_GLOBAL) {
                    scanIp++;
                }
                else if (op == OpCode::OP_CLOSURE) {
                    scanIp += 2;
                    // Skip upvalue info
                    if (scanIp + 1 < codeSize) {
                        uint16_t numUpvalues = (chunk->code[scanIp] << 8) | chunk->code[scanIp + 1];
                        scanIp += 2 + numUpvalues * 2;
                    }
                }
                // All other opcodes have no operands
                break;
        }
    }
    isJumpTarget[0] = true; // Always need entry point

    // Second pass: generate code with labels only at jump targets
    ip = 0;
    while (ip < codeSize) {
        // Record source location for debug mapping
        size_t instrOffset = ip;

        // Emit label if this is a jump target
        if (isJumpTarget[ip] && ip != 0) {
            code << "    instr_" << ip << ":;\n";
        }

        size_t instrStart = ip;
        uint8_t instruction = readByte();
        OpCode op = static_cast<OpCode>(instruction);

        // Record source location for debugging
        recordSourceLocation(instrOffset);
        
        // Emit debug comment with bytecode offset
        if (generateDebugSymbols) {
            code << "    // @bytecode:" << instrOffset << "\n";
        }

        (void)instrStart;  // For debug output if needed

        switch (op) {
            case OpCode::OP_RETURN:
                code << "    // RETURN\n";
                code << "    return 0;\n\n";
                break;
                
            case OpCode::OP_CONSTANT: {
                uint8_t index = readByte();
                code << "    // CONSTANT " << static_cast<int>(index) << "\n";
                if (index >= chunk->constants.size()) {
                    code << "    // ERROR: index " << static_cast<int>(index) << " out of bounds (size=" << chunk->constants.size() << ")\n";
                    code << "    stack[sp++] = Value();\n\n";
                } else {
                    code << "    stack[sp++] = constants[" << static_cast<int>(index) << "];\n\n";
                }
                break;
            }
            
            case OpCode::OP_CONSTANT_LONG: {
                uint16_t index = readShort();
                code << "    // CONSTANT_LONG " << index << "\n";
                if (index >= chunk->constants.size()) {
                    code << "    // ERROR: index " << index << " out of bounds (size=" << chunk->constants.size() << ")\n";
                    code << "    stack[sp++] = Value();\n\n";
                } else {
                    code << "    stack[sp++] = constants[" << index << "];\n\n";
                }
                break;
            }
            
            case OpCode::OP_NIL:
                code << "    // NIL\n";
                code << "    stack[sp++] = Value();\n\n";
                break;
                
            case OpCode::OP_TRUE:
                code << "    // TRUE\n";
                code << "    stack[sp++] = Value(true);\n\n";
                break;
                
            case OpCode::OP_FALSE:
                code << "    // FALSE\n";
                code << "    stack[sp++] = Value(false);\n\n";
                break;
                
            case OpCode::OP_POP:
                code << "    // POP\n";
                code << "    sp--;\n\n";
                break;
                
            case OpCode::OP_DUP:
                code << "    // DUP\n";
                code << "    stack[sp] = stack[sp-1]; sp++;\n\n";
                break;
                
            case OpCode::OP_GET_LOCAL: {
                uint8_t slot = readByte();
                code << "    // GET_LOCAL " << static_cast<int>(slot) << "\n";
                code << "    stack[sp++] = locals[" << static_cast<int>(slot) << "];\n\n";
                break;
            }
            
            case OpCode::OP_SET_LOCAL: {
                uint8_t slot = readByte();
                code << "    // SET_LOCAL " << static_cast<int>(slot) << "\n";
                code << "    locals[" << static_cast<int>(slot) << "] = stack[--sp];\n\n";
                break;
            }
            
            case OpCode::OP_GET_GLOBAL: {
                uint8_t nameIndex = readByte();  // Compiler uses 1-byte when possible
                if (nameIndex >= chunk->constants.size()) {
                    code << "    // GET_GLOBAL: invalid constant index\n";
                    code << "    stack[sp++] = Value();\n\n";
                } else {
                    Value nameValue = chunk->constants[nameIndex];
                    if (nameValue.type == ValueType::OBJ_STRING) {
                        std::string name = nameValue.as.obj_string->chars;
                        auto it = globalSlotMap.find(name);
                        if (it != globalSlotMap.end()) {
                            code << "    // GET_GLOBAL " << name << "\n";
                            code << "    stack[sp++] = global_" << name << ";\n\n";
                        } else {
                            code << "    // GET_GLOBAL: undefined variable " << name << "\n";
                            code << "    stack[sp++] = Value();\n\n";
                        }
                    } else {
                        code << "    // GET_GLOBAL: invalid name type\n";
                        code << "    stack[sp++] = Value();\n\n";
                    }
                }
                break;
            }

            case OpCode::OP_SET_GLOBAL: {
                uint8_t nameIndex = readByte();  // Compiler uses 1-byte when possible
                if (nameIndex >= chunk->constants.size()) {
                    code << "    // SET_GLOBAL: invalid constant index\n";
                    code << "    sp--;\n\n";
                } else {
                    Value nameValue = chunk->constants[nameIndex];
                    if (nameValue.type == ValueType::OBJ_STRING) {
                        std::string name = nameValue.as.obj_string->chars;
                        auto it = globalSlotMap.find(name);
                        if (it != globalSlotMap.end()) {
                            code << "    // SET_GLOBAL " << name << "\n";
                            code << "    global_" << name << " = stack[--sp];\n\n";
                        } else {
                            code << "    // SET_GLOBAL: undefined variable " << name << "\n";
                            code << "    sp--;\n\n";
                        }
                    } else {
                        code << "    // SET_GLOBAL: invalid name type\n";
                        code << "    sp--;\n\n";
                    }
                }
                break;
            }
                
            case OpCode::OP_ADD:
                code << "    // ADD\n";
                code << "    { Value b = stack[--sp]; Value a = stack[--sp];\n";
                code << "      if (a.type == ValueType::NUMBER && b.type == ValueType::NUMBER)\n";
                code << "        stack[sp++] = Value(a.as.number + b.as.number);\n";
                code << "      else if (a.type == ValueType::OBJ_STRING || b.type == ValueType::OBJ_STRING)\n";
                code << "        { static std::string s; s = a.toString() + b.toString(); stack[sp++] = Value(s.c_str()); }\n";
                code << "      else stack[sp++] = Value(); }\n\n";
                break;
                
            case OpCode::OP_SUBTRACT:
                code << "    // SUBTRACT\n";
                code << "    { Value b = stack[--sp]; Value a = stack[--sp];\n";
                code << "      stack[sp++] = Value(a.as.number - b.as.number); }\n\n";
                break;
                
            case OpCode::OP_MULTIPLY:
                code << "    // MULTIPLY\n";
                code << "    { Value b = stack[--sp]; Value a = stack[--sp];\n";
                code << "      stack[sp++] = Value(a.as.number * b.as.number); }\n\n";
                break;
                
            case OpCode::OP_DIVIDE:
                code << "    // DIVIDE\n";
                code << "    { Value b = stack[--sp]; Value a = stack[--sp];\n";
                code << "      stack[sp++] = Value(a.as.number / b.as.number); }\n\n";
                break;
                
            case OpCode::OP_MODULO:
                code << "    // MODULO\n";
                code << "    { Value b = stack[--sp]; Value a = stack[--sp];\n";
                code << "      stack[sp++] = Value(std::fmod(a.as.number, b.as.number)); }\n\n";
                break;
                
            case OpCode::OP_NEGATE:
                code << "    // NEGATE\n";
                code << "    { Value a = stack[--sp];\n";
                code << "      stack[sp++] = Value(-a.as.number); }\n\n";
                break;
                
            case OpCode::OP_NOT:
                code << "    // NOT\n";
                code << "    { Value a = stack[--sp];\n";
                code << "      bool truthy = (a.type == ValueType::BOOLEAN && a.as.boolean) || (a.type == ValueType::NUMBER && a.as.number != 0.0);\n";
                code << "      stack[sp++] = Value(!truthy); }\n\n";
                break;
                
            case OpCode::OP_EQUAL:
                code << "    // EQUAL\n";
                code << "    { Value b = stack[--sp]; Value a = stack[--sp];\n";
                code << "      bool eq = (a.type == b.type && a.as.number == b.as.number);\n";
                code << "      stack[sp++] = Value(eq); }\n\n";
                break;
                
            case OpCode::OP_NOT_EQUAL:
                code << "    // NOT_EQUAL\n";
                code << "    { Value b = stack[--sp]; Value a = stack[--sp];\n";
                code << "      bool eq = (a.type == b.type && a.as.number == b.as.number);\n";
                code << "      stack[sp++] = Value(!eq); }\n\n";
                break;
                
            case OpCode::OP_GREATER:
                code << "    // GREATER\n";
                code << "    { Value b = stack[--sp]; Value a = stack[--sp];\n";
                code << "      stack[sp++] = Value(a.as.number > b.as.number); }\n\n";
                break;
                
            case OpCode::OP_LESS:
                code << "    // LESS\n";
                code << "    { Value b = stack[--sp]; Value a = stack[--sp];\n";
                code << "      stack[sp++] = Value(a.as.number < b.as.number); }\n\n";
                break;
                
            case OpCode::OP_GREATER_INT:
                code << "    // GREATER_INT\n";
                code << "    { Value b = stack[--sp]; Value a = stack[--sp];\n";
                code << "      stack[sp++] = Value(a.as.number > b.as.number); }\n\n";
                break;
                
            case OpCode::OP_LESS_INT:
                code << "    // LESS_INT\n";
                code << "    { Value b = stack[--sp]; Value a = stack[--sp];\n";
                code << "      stack[sp++] = Value(a.as.number < b.as.number); }\n\n";
                break;
                
            case OpCode::OP_ADD_INT:
                code << "    // ADD_INT\n";
                code << "    { Value b = stack[--sp]; Value a = stack[--sp];\n";
                code << "      stack[sp++] = Value(a.as.number + b.as.number); }\n\n";
                break;
                
            case OpCode::OP_SUB_INT:
                code << "    // SUB_INT\n";
                code << "    { Value b = stack[--sp]; Value a = stack[--sp];\n";
                code << "      stack[sp++] = Value(a.as.number - b.as.number); }\n\n";
                break;
                
            case OpCode::OP_MUL_INT:
                code << "    // MUL_INT\n";
                code << "    { Value b = stack[--sp]; Value a = stack[--sp];\n";
                code << "      stack[sp++] = Value(a.as.number * b.as.number); }\n\n";
                break;
                
            case OpCode::OP_DIV_INT:
                code << "    // DIV_INT\n";
                code << "    { Value b = stack[--sp]; Value a = stack[--sp];\n";
                code << "      stack[sp++] = Value(a.as.number / b.as.number); }\n\n";
                break;
                
            case OpCode::OP_MOD_INT:
                code << "    // MOD_INT\n";
                code << "    { Value b = stack[--sp]; Value a = stack[--sp];\n";
                code << "      stack[sp++] = Value(std::fmod(a.as.number, b.as.number)); }\n\n";
                break;
                
            case OpCode::OP_NEGATE_INT:
                code << "    // NEGATE_INT\n";
                code << "    { Value a = stack[--sp];\n";
                code << "      stack[sp++] = Value(-a.as.number); }\n\n";
                break;
                
            case OpCode::OP_EQUAL_INT:
                code << "    // EQUAL_INT\n";
                code << "    { Value b = stack[--sp]; Value a = stack[--sp];\n";
                code << "      stack[sp++] = Value(a.as.number == b.as.number); }\n\n";
                break;
                
            case OpCode::OP_BITWISE_AND:
                code << "    // BITWISE_AND\n";
                code << "    { Value b = stack[--sp]; Value a = stack[--sp];\n";
                code << "      stack[sp++] = Value((double)((long long)a.as.number & (long long)b.as.number)); }\n\n";
                break;
                
            case OpCode::OP_BITWISE_OR:
                code << "    // BITWISE_OR\n";
                code << "    { Value b = stack[--sp]; Value a = stack[--sp];\n";
                code << "      stack[sp++] = Value((double)((long long)a.as.number | (long long)b.as.number)); }\n\n";
                break;
                
            case OpCode::OP_BITWISE_XOR:
                code << "    // BITWISE_XOR\n";
                code << "    { Value b = stack[--sp]; Value a = stack[--sp];\n";
                code << "      stack[sp++] = Value((double)((long long)a.as.number ^ (long long)b.as.number)); }\n\n";
                break;
                
            case OpCode::OP_BITWISE_NOT:
                code << "    // BITWISE_NOT\n";
                code << "    { Value a = stack[--sp];\n";
                code << "      stack[sp++] = Value((double)(~(long long)a.as.number)); }\n\n";
                break;
                
            case OpCode::OP_LEFT_SHIFT:
                code << "    // LEFT_SHIFT\n";
                code << "    { Value b = stack[--sp]; Value a = stack[--sp];\n";
                code << "      stack[sp++] = Value((double)((long long)a.as.number << (long long)b.as.number)); }\n\n";
                break;
                
            case OpCode::OP_RIGHT_SHIFT:
                code << "    // RIGHT_SHIFT\n";
                code << "    { Value b = stack[--sp]; Value a = stack[--sp];\n";
                code << "      stack[sp++] = Value((double)((long long)a.as.number >> (long long)b.as.number)); }\n\n";
                break;
                
            case OpCode::OP_SAY:
                code << "    // SAY\n";
                code << "    { Value a = stack[--sp]; std::cout << a.toString() << std::endl; }\n\n";
                break;
                
            case OpCode::OP_JUMP: {
                uint16_t offset = readShort();
                code << "    // JUMP " << offset << " (goto)\n";
                code << "    goto instr_" << (ip + offset) << ";\n\n";
                break;
            }
            
            case OpCode::OP_JUMP_IF_FALSE: {
                uint16_t offset = readShort();
                code << "    // JUMP_IF_FALSE " << offset << "\n";
                code << "    if (!stack[sp-1].as.boolean && stack[sp-1].type != ValueType::NUMBER) goto instr_" << (ip + offset) << ";\n\n";
                break;
            }
            
            case OpCode::OP_LOOP: {
                uint16_t offset = readShort();
                code << "    // LOOP " << offset << "\n";
                code << "    goto instr_" << (ip > offset ? ip - offset : 0) << ";\n\n";
                break;
            }
            
            case OpCode::OP_LOOP_IF_LESS_LOCAL: {
                uint8_t slot = readByte();
                uint8_t constIdx = readByte();  // Constant index (was missing!)
                uint16_t offset = readShort();
                code << "    // LOOP_IF_LESS_LOCAL slot=" << static_cast<int>(slot) << " const=" << static_cast<int>(constIdx) << " offset=" << offset << "\n";
                code << "    if (!(locals[" << static_cast<int>(slot) << "].as.number < constants[" << static_cast<int>(constIdx) << "].as.number)) goto instr_" << (ip + offset) << ";\n\n";
                break;
            }
            
            case OpCode::OP_LESS_JUMP: {
                uint16_t offset = readShort();
                code << "    // LESS_JUMP " << offset << "\n";
                code << "    { Value b = stack[--sp]; Value a = stack[--sp];\n";
                code << "      if (!(a.as.number < b.as.number)) goto instr_" << (ip + offset) << "; }\n\n";
                break;
            }
            
            case OpCode::OP_GREATER_JUMP: {
                uint16_t offset = readShort();
                code << "    // GREATER_JUMP " << offset << "\n";
                code << "    { Value b = stack[--sp]; Value a = stack[--sp];\n";
                code << "      if (!(a.as.number > b.as.number)) goto instr_" << (ip + offset) << "; }\n\n";
                break;
            }
            
            case OpCode::OP_EQUAL_JUMP: {
                uint16_t offset = readShort();
                code << "    // EQUAL_JUMP " << offset << "\n";
                code << "    { Value b = stack[--sp]; Value a = stack[--sp];\n";
                code << "      if (!(a.as.number == b.as.number)) goto instr_" << (ip + offset) << "; }\n\n";
                break;
            }
            
            case OpCode::OP_CALL: {
                uint8_t argCount = readByte();
                // For simple cases where function is known at compile time,
                // we can generate a direct call. For now, use interpreter fallback.
                code << "    // CALL " << static_cast<int>(argCount) << " args\n";
                code << "    sp -= " << static_cast<int>(argCount + 1) << ";\n";
                code << "    stack[sp++] = Value();  // Interpreter fallback\n\n";
                break;
            }

            case OpCode::OP_CALL_FAST: {
                uint8_t argCount = readByte();
                code << "    // CALL_FAST " << static_cast<int>(argCount) << " args\n";
                code << "    sp -= " << static_cast<int>(argCount + 1) << ";\n";
                code << "    stack[sp++] = Value();\n\n";
                break;
            }
            
            case OpCode::OP_CLOSURE:
                // Skip - closures not supported in AOT v1
                readByte(); // function index
                code << "    // CLOSURE (not supported in AOT v1)\n\n";
                break;
                
            case OpCode::OP_GET_UPVALUE:
            case OpCode::OP_SET_UPVALUE:
            case OpCode::OP_CLOSE_UPVALUE:
                // Skip - upvalues not supported in AOT v1
                readByte();
                code << "    // UPVALUE (not supported in AOT v1)\n\n";
                break;

            case OpCode::OP_ARRAY: {
                // Basic array support with static allocation pool
                uint8_t arraySize = readByte();
                std::string arrayName = "arr" + std::to_string(arrayCounter++);
                staticArrays.push_back({arrayName, arraySize});
                
                code << "    // ARRAY size=" << static_cast<int>(arraySize) << " -> " << arrayName << "\n";
                code << "    // Pop " << static_cast<int>(arraySize) << " elements from stack into static array\n";
                code << "    {\n";
                code << "        Value temp[" << static_cast<int>(arraySize) << "];\n";
                code << "        for (int i = " << static_cast<int>(arraySize) << " - 1; i >= 0; i--) temp[i] = stack[--sp];\n";
                code << "        for (int i = 0; i < " << static_cast<int>(arraySize) << "; i++) array_" << arrayName << "[i] = temp[i];\n";
                code << "        // Push array reference (simplified - just push first element's address as marker)\n";
                code << "        stack[sp++] = Value(\"array_" << arrayName << "\");\n";
                code << "    }\n\n";
                break;
            }
            
            case OpCode::OP_OBJECT:
            case OpCode::OP_INDEX_GET:
            case OpCode::OP_INDEX_SET:
            case OpCode::OP_GET_PROPERTY:
            case OpCode::OP_SET_PROPERTY:
            case OpCode::OP_THIS:
            case OpCode::OP_INVOKE:
                // Complex types not supported in AOT v1
                code << "    // COMPLEX TYPE OP (not supported in AOT v1)\n\n";
                // Skip any operands
                if (op == OpCode::OP_GET_PROPERTY || op == OpCode::OP_SET_PROPERTY ||
                    op == OpCode::OP_INVOKE) {
                    readByte(); // property name index
                }
                if (op == OpCode::OP_INVOKE) {
                    readByte(); // arg count
                }
                break;

            case OpCode::OP_DEFINE_GLOBAL:
            case OpCode::OP_DEFINE_TYPED_GLOBAL: {
                uint8_t nameIndex = readByte();  // Compiler uses 1-byte index when possible

                if (nameIndex < chunk->constants.size()) {
                    Value nameValue = chunk->constants[nameIndex];
                    if (nameValue.type == ValueType::OBJ_STRING) {
                        std::string name = nameValue.as.obj_string->chars;
                        auto it = globalSlotMap.find(name);
                        if (it != globalSlotMap.end()) {
                            code << "    // DEFINE_GLOBAL " << name << "\n";
                            code << "    global_" << name << " = stack[--sp];\n\n";
                        }
                    }
                }
                break;
            }

            case OpCode::OP_SET_GLOBAL_TYPED: {
                uint8_t nameIndex = readByte();
                if (nameIndex >= chunk->constants.size()) {
                    code << "    // SET_GLOBAL_TYPED: invalid constant index\n";
                    code << "    sp--;\n\n";
                } else {
                    Value nameValue = chunk->constants[nameIndex];
                    if (nameValue.type == ValueType::OBJ_STRING) {
                        std::string name = nameValue.as.obj_string->chars;
                        auto it = globalSlotMap.find(name);
                        if (it != globalSlotMap.end()) {
                            code << "    // SET_GLOBAL_TYPED " << name << "\n";
                            code << "    global_" << name << " = stack[--sp];\n\n";
                        } else {
                            code << "    // SET_GLOBAL_TYPED: undefined variable " << name << "\n";
                            code << "    sp--;\n\n";
                        }
                    } else {
                        code << "    // SET_GLOBAL_TYPED: invalid name type\n";
                        code << "    sp--;\n\n";
                    }
                }
                break;
            }

            case OpCode::OP_SET_LOCAL_TYPED: {
                uint8_t slot = readByte();
                code << "    // SET_LOCAL_TYPED " << static_cast<int>(slot) << "\n";
                code << "    locals[" << slot << "] = stack[--sp];\n\n";
                break;
            }

            case OpCode::OP_GET_GLOBAL_FAST: {
                uint8_t slot = readByte();
                // Find global name by slot
                std::string name;
                for (const auto& pair : globalSlotMap) {
                    if (pair.second == (int)slot) {
                        name = pair.first;
                        break;
                    }
                }
                if (!name.empty()) {
                    code << "    // GET_GLOBAL_FAST " << name << "\n";
                    code << "    stack[sp++] = global_" << name << ";\n\n";
                } else {
                    code << "    // GET_GLOBAL_FAST: unknown slot " << static_cast<int>(slot) << "\n";
                    code << "    stack[sp++] = Value();\n\n";
                }
                break;
            }

            case OpCode::OP_SET_GLOBAL_FAST: {
                uint8_t slot = readByte();
                // Find global name by slot
                std::string name;
                for (const auto& pair : globalSlotMap) {
                    if (pair.second == (int)slot) {
                        name = pair.first;
                        break;
                    }
                }
                if (!name.empty()) {
                    code << "    // SET_GLOBAL_FAST " << name << "\n";
                    code << "    global_" << name << " = stack[--sp];\n\n";
                } else {
                    code << "    // SET_GLOBAL_FAST: unknown slot " << static_cast<int>(slot) << "\n";
                    code << "    sp--;\n\n";
                }
                break;
            }

            case OpCode::OP_LOAD_LOCAL_0: {
                code << "    // LOAD_LOCAL_0\n";
                code << "    stack[sp++] = locals[0];\n\n";
                break;
            }
            
            case OpCode::OP_LOAD_LOCAL_1: {
                code << "    // LOAD_LOCAL_1\n";
                code << "    stack[sp++] = locals[1];\n\n";
                break;
            }
            
            case OpCode::OP_LOAD_LOCAL_2: {
                code << "    // LOAD_LOCAL_2\n";
                code << "    stack[sp++] = locals[2];\n\n";
                break;
            }
            
            case OpCode::OP_LOAD_LOCAL_3: {
                code << "    // LOAD_LOCAL_3\n";
                code << "    stack[sp++] = locals[3];\n\n";
                break;
            }
            
            case OpCode::OP_CONST_ZERO: {
                code << "    // CONST_ZERO\n";
                code << "    stack[sp++] = Value(0.0);\n\n";
                break;
            }
            
            case OpCode::OP_CONST_ONE: {
                code << "    // CONST_ONE\n";
                code << "    stack[sp++] = Value(1.0);\n\n";
                break;
            }
            
            case OpCode::OP_CONST_INT8: {
                uint8_t value = readByte();
                code << "    // CONST_INT8 " << static_cast<int>(value) << "\n";
                code << "    stack[sp++] = Value(" << static_cast<int>(value) << ".0);\n\n";
                break;
            }
            
            case OpCode::OP_INC_LOCAL_INT: {
                uint8_t slot = readByte();
                code << "    // INC_LOCAL_INT " << static_cast<int>(slot) << "\n";
                code << "    locals[" << static_cast<int>(slot) << "].as.number += 1.0;\n\n";
                break;
            }
            
            case OpCode::OP_DEC_LOCAL_INT: {
                uint8_t slot = readByte();
                code << "    // DEC_LOCAL_INT " << static_cast<int>(slot) << "\n";
                code << "    locals[" << static_cast<int>(slot) << "].as.number -= 1.0;\n\n";
                break;
            }
            
            case OpCode::OP_ADD_LOCAL_CONST: {
                uint8_t slot = readByte();
                uint8_t constIndex = readByte();
                code << "    // ADD_LOCAL_CONST slot=" << static_cast<int>(slot) << " const=" << constIndex << "\n";
                code << "    locals[" << static_cast<int>(slot) << "].as.number += constants[" << constIndex << "].as.number;\n\n";
                break;
            }

            case OpCode::OP_INCREMENT_LOCAL: {
                uint8_t slot = readByte();
                code << "    // INCREMENT_LOCAL " << static_cast<int>(slot) << "\n";
                code << "    locals[" << static_cast<int>(slot) << "].as.number += 1.0;\n\n";
                break;
            }

            case OpCode::OP_DECREMENT_LOCAL: {
                uint8_t slot = readByte();
                code << "    // DECREMENT_LOCAL " << static_cast<int>(slot) << "\n";
                code << "    locals[" << static_cast<int>(slot) << "].as.number -= 1.0;\n\n";
                break;
            }
            
            case OpCode::OP_INCREMENT_GLOBAL: {
                uint8_t nameIndex = readByte();  // Compiler uses 1-byte when possible

                if (nameIndex < chunk->constants.size()) {
                    Value nameValue = chunk->constants[nameIndex];
                    if (nameValue.type == ValueType::OBJ_STRING) {
                        std::string name = nameValue.as.obj_string->chars;
                        auto it = globalSlotMap.find(name);
                        if (it != globalSlotMap.end()) {
                            code << "    // INCREMENT_GLOBAL " << name << "\n";
                            code << "    global_" << name << ".as.number += 1.0;\n\n";
                        }
                    }
                }
                break;
            }

            case OpCode::OP_TYPE_GUARD:
            case OpCode::OP_LOOP_HINT:
            case OpCode::OP_TAIL_CALL:
                // JIT hints - not needed in AOT
                code << "    // " << static_cast<int>(op) << " (JIT hint - no-op in AOT)\n\n";
                break;

            case OpCode::OP_BREAK:
            case OpCode::OP_CONTINUE:
                // Loop control - handled by structured control flow in AOT
                code << "    // " << static_cast<int>(op) << " (loop control - handled by structured flow)\n\n";
                break;

            case OpCode::OP_TRY:
            case OpCode::OP_END_TRY:
                // Exception handling frames - not implemented in AOT v1
                code << "    // " << static_cast<int>(op) << " (exception frames - no-op in AOT v1)\n\n";
                break;

            case OpCode::OP_THROW: {
                // =================================================================
                // OP_THROW - Exception Throwing
                // Throws a runtime exception with the value on top of stack
                // =================================================================
                code << "    // THROW\n";
                code << "    {\n";
                code << "        Value exc = stack[--sp];\n";
                code << "        std::cerr << \"RUNTIME ERROR: \" << exc.toString() << std::endl;\n";
                code << "        std::cerr << \"  at bytecode offset \" << ip << std::endl;\n";
                code << "        return Value();  // Return nil to indicate error\n";
                code << "    }\n\n";
                break;
            }

            case OpCode::OP_LOGICAL_AND: {
                // =================================================================
                // OP_LOGICAL_AND - Short-Circuit Logical AND
                // Evaluates second operand only if first is truthy
                // Pops both operands, pushes result
                // =================================================================
                uint16_t offset = readShort();  // Jump offset for short-circuit
                code << "    // LOGICAL_AND (short-circuit)\n";
                code << "    {\n";
                code << "        Value b = stack[--sp];\n";
                code << "        Value a = stack[--sp];\n";
                code << "        // Short-circuit: if a is falsy, result is a (don't evaluate b)\n";
                code << "        if (a.type == ValueType::NIL || (a.type == ValueType::BOOLEAN && !a.as.boolean)) {\n";
                code << "            stack[sp++] = a;  // Result is a (falsy)\n";
                code << "            goto instr_" << (ip + offset) << ";  // Skip b evaluation\n";
                code << "        }\n";
                code << "        // Both truthy - result is b\n";
                code << "        stack[sp++] = b;\n";
                code << "    }\n\n";
                break;
            }

            case OpCode::OP_LOGICAL_OR: {
                // =================================================================
                // OP_LOGICAL_OR - Short-Circuit Logical OR
                // Evaluates second operand only if first is falsy
                // Pops both operands, pushes result
                // =================================================================
                uint16_t offset = readShort();  // Jump offset for short-circuit
                code << "    // LOGICAL_OR (short-circuit)\n";
                code << "    {\n";
                code << "        Value b = stack[--sp];\n";
                code << "        Value a = stack[--sp];\n";
                code << "        // Short-circuit: if a is truthy, result is a (don't evaluate b)\n";
                code << "        if (a.type != ValueType::NIL && (a.type != ValueType::BOOLEAN || a.as.boolean)) {\n";
                code << "            stack[sp++] = a;  // Result is a (truthy)\n";
                code << "            goto instr_" << (ip + offset) << ";  // Skip b evaluation\n";
                code << "        }\n";
                code << "        // Both falsy - result is b\n";
                code << "        stack[sp++] = b;\n";
                code << "    }\n\n";
                break;
            }

            case OpCode::OP_VALIDATE_SAFE_FUNCTION:
            case OpCode::OP_VALIDATE_SAFE_VARIABLE:
            case OpCode::OP_VALIDATE_SAFE_FILE_FUNCTION:
            case OpCode::OP_VALIDATE_SAFE_FILE_VARIABLE:
                // =================================================================
                // Safe Mode Validation - No-op in AOT
                // These opcodes are for runtime safe mode validation.
                // AOT-compiled code is already validated at compile time,
                // so these can be safely skipped.
                // =================================================================
                code << "    // " << static_cast<int>(op) << " (safe mode - no-op in AOT)\n\n";
                // Skip operands
                break;
                
            case OpCode::OP_COUNT:
                break;
        }
    }
}

std::string AotCompiler::generateCode(const std::string& functionName) {
    code.str("");
    code.clear();

    // First pass: collect global variable definitions
    collectGlobalDefinitions();
    
    // Generate prologue (includes globals) - but NOT the main function yet
    code << "#include <iostream>\n";
    code << "#include <cmath>\n";
    code << "#include <string>\n";
    code << "#include <cstdint>\n\n";

    // Minimal Value struct for AOT execution
    code << "enum class ValueType { NIL, BOOLEAN, NUMBER, OBJ_STRING, CALLABLE };\n\n";

    code << "struct Value {\n";
    code << "    ValueType type;\n";
    code << "    union { bool boolean; double number; const char* string; } as;\n";
    code << "    \n";
    code << "    Value() : type(ValueType::NIL) { as.number = 0; }\n";
    code << "    Value(bool b) : type(ValueType::BOOLEAN) { as.boolean = b; }\n";
    code << "    Value(double n) : type(ValueType::NUMBER) { as.number = n; }\n";
    code << "    Value(const char* s) : type(ValueType::OBJ_STRING) { as.string = s; }\n";
    code << "    \n";
    code << "    std::string toString() const {\n";
    code << "        switch (type) {\n";
    code << "            case ValueType::NIL: return \"nil\";\n";
    code << "            case ValueType::BOOLEAN: return as.boolean ? \"true\" : \"false\";\n";
    code << "            case ValueType::NUMBER: {\n";
    code << "                double n = as.number;\n";
    code << "                if (n == (long long)n) return std::to_string((long long)n);\n";
    code << "                return std::to_string(n);\n";
    code << "            }\n";
    code << "            case ValueType::OBJ_STRING: return std::string(as.string);\n";
    code << "            default: return \"unknown\";\n";
    code << "        }\n";
    code << "    }\n";
    code << "};\n\n";

    // Constants array
    code << "// Embedded constants\n";
    code << "static const Value constants[] = {\n";
    for (size_t i = 0; i < chunk->constants.size(); i++) {
        code << "    " << constantToCpp(i) << ",\n";
    }
    code << "};\n\n";

    // Global variables
    if (!globalNames.empty()) {
        code << "// Global variables\n";
        for (const auto& name : globalNames) {
            code << "static Value global_" << name << ";\n";
        }
        code << "\n";
    }

    // Generate all nested functions BEFORE main function
    for (size_t i = 0; i < chunk->constants.size(); i++) {
        const Value& v = chunk->constants[i];
        if (v.type == ValueType::CALLABLE) {
            const Function* func = dynamic_cast<const Function*>(v.as.callable);
            if (func && func->chunk) {
                std::string funcName = "aot_func" + std::to_string(i);
                code << generateFunctionCode(func->chunk, funcName, func->arity_val);
            }
        }
    }
    
    // Now generate main function
    code << "int " << functionName << "() {\n";
    code << "    // Stack and locals\n";
    code << "    Value stack[256];\n";
    code << "    Value locals[256];\n";
    code << "    int sp = 0;  // Stack pointer\n";
    code << "    (void)stack; (void)locals; (void)sp;  // Suppress unused warnings\n\n";
    
    // Generate main function body
    generateBytecodeBody();
    generateEpilogue();

    return code.str();
}

// Generate C++ function from a chunk
std::string AotCompiler::generateFunctionCode(const Chunk* funcChunk, const std::string& funcName, int paramCount) {
    std::ostringstream funcCode;
    
    funcCode << "\n// AOT-compiled function: " << funcName << " (params: " << paramCount << ")\n";
    funcCode << "static Value " << funcName << "_impl(";
    for (int i = 0; i < paramCount; i++) {
        if (i > 0) funcCode << ", ";
        funcCode << "Value p" << i;
    }
    funcCode << ") {\n";
    funcCode << "    Value locals[256];\n";
    funcCode << "    (void)locals;\n";  // Suppress unused warning if no locals used
    
    // Initialize parameters in locals
    for (int i = 0; i < paramCount; i++) {
        funcCode << "    locals[" << i << "] = p" << i << ";\n";
    }
    
    // Generate function body inline (no separate stack/sp needed for simple functions)
    AotCompiler funcCompiler(funcChunk);
    funcCompiler.ip = 0;
    size_t codeSize = funcChunk->code.size();
    
    // First pass: find jump targets
    std::vector<bool> isJumpTarget(codeSize + 1, false);
    size_t scanIp = 0;
    while (scanIp < codeSize) {
        size_t instrStart = scanIp;
        uint8_t instruction = funcChunk->code[scanIp++];
        OpCode op = static_cast<OpCode>(instruction);

        switch (op) {
            case OpCode::OP_JUMP:
            case OpCode::OP_JUMP_IF_FALSE:
            case OpCode::OP_LOOP:
            case OpCode::OP_LESS_JUMP:
            case OpCode::OP_GREATER_JUMP:
            case OpCode::OP_EQUAL_JUMP: {
                uint16_t offset = (funcChunk->code[scanIp] << 8) | funcChunk->code[scanIp + 1];
                scanIp += 2;
                size_t target = (op == OpCode::OP_LOOP) ? (instrStart - offset) : (scanIp + offset);
                if (target <= codeSize) isJumpTarget[target] = true;
                break;
            }
            case OpCode::OP_LOOP_IF_LESS_LOCAL: {
                scanIp++; // slot
                scanIp++; // constant index
                uint16_t offset = (funcChunk->code[scanIp] << 8) | funcChunk->code[scanIp + 1];
                scanIp += 2;
                // Forward jump to exit loop (when condition is false)
                size_t target = scanIp + offset;
                if (target <= codeSize) isJumpTarget[target] = true;
                break;
            }
            default:
                if (op == OpCode::OP_CONSTANT || op == OpCode::OP_GET_LOCAL ||
                    op == OpCode::OP_SET_LOCAL || op == OpCode::OP_GET_UPVALUE ||
                    op == OpCode::OP_SET_UPVALUE || op == OpCode::OP_CLOSE_UPVALUE ||
                    op == OpCode::OP_GET_PROPERTY || op == OpCode::OP_SET_PROPERTY ||
                    op == OpCode::OP_GET_GLOBAL || op == OpCode::OP_SET_GLOBAL ||
                    op == OpCode::OP_INCREMENT_LOCAL || op == OpCode::OP_DECREMENT_LOCAL ||
                    op == OpCode::OP_INC_LOCAL_INT || op == OpCode::OP_DEC_LOCAL_INT ||
                    op == OpCode::OP_CONST_INT8 || op == OpCode::OP_CALL ||
                    op == OpCode::OP_CALL_FAST || op == OpCode::OP_ARRAY ||
                    op == OpCode::OP_INVOKE) {
                    scanIp++;
                } else if (op == OpCode::OP_CONSTANT_LONG || op == OpCode::OP_ADD_LOCAL_CONST) {
                    scanIp += 2;
                } else if (op == OpCode::OP_INCREMENT_GLOBAL) {
                    scanIp += 2;
                } else if (op == OpCode::OP_CLOSURE) {
                    scanIp += 2;
                    if (scanIp + 1 < codeSize) {
                        uint16_t numUpvalues = (funcChunk->code[scanIp] << 8) | funcChunk->code[scanIp + 1];
                        scanIp += 2 + numUpvalues * 2;
                    }
                }
                break;
        }
    }
    isJumpTarget[0] = true;
    
    // Second pass: generate code with stack simulation
    funcCode << "    Value stack[256];\n";
    funcCode << "    int sp = 0;\n";
    funcCode << "    (void)stack; (void)sp;\n\n";
    
    funcCompiler.ip = 0;
    while (funcCompiler.ip < codeSize) {
        if (isJumpTarget[funcCompiler.ip] && funcCompiler.ip != 0) {
            funcCode << "    instr_" << funcCompiler.ip << ":;\n";
        }
        
        uint8_t instruction = funcCompiler.readByte();
        OpCode op = static_cast<OpCode>(instruction);

        switch (op) {
            case OpCode::OP_RETURN:
                funcCode << "    return locals[0];\n\n";
                break;
            case OpCode::OP_CONSTANT: {
                uint8_t index = funcCompiler.readByte();
                funcCode << "    locals[0] = constants[" << static_cast<int>(index) << "];\n";
                break;
            }
            case OpCode::OP_GET_LOCAL: {
                uint8_t slot = funcCompiler.readByte();
                funcCode << "    locals[0] = locals[" << static_cast<int>(slot) << "];\n";
                break;
            }
            case OpCode::OP_SET_LOCAL: {
                uint8_t slot = funcCompiler.readByte();
                funcCode << "    locals[" << static_cast<int>(slot) << "] = locals[0];\n";
                break;
            }
            case OpCode::OP_ADD:
                funcCode << "    locals[0].as.number += locals[1].as.number;\n";
                break;
            case OpCode::OP_SUBTRACT:
                funcCode << "    locals[0].as.number -= locals[1].as.number;\n";
                break;
            case OpCode::OP_MULTIPLY:
                funcCode << "    locals[0].as.number *= locals[1].as.number;\n";
                break;
            case OpCode::OP_DIVIDE:
                funcCode << "    locals[0].as.number /= locals[1].as.number;\n";
                break;
            case OpCode::OP_LESS:
                funcCode << "    locals[0] = Value(locals[0].as.number < locals[1].as.number);\n";
                break;
            case OpCode::OP_GREATER:
                funcCode << "    locals[0] = Value(locals[0].as.number > locals[1].as.number);\n";
                break;
            case OpCode::OP_EQUAL:
                funcCode << "    locals[0] = Value(locals[0].as.number == locals[1].as.number);\n";
                break;
            case OpCode::OP_NOT:
                funcCode << "    locals[0] = Value(!locals[0].as.number);\n";
                break;
            case OpCode::OP_NEGATE:
                funcCode << "    locals[0].as.number = -locals[0].as.number;\n";
                break;
            case OpCode::OP_JUMP: {
                uint16_t offset = funcCompiler.readShort();
                funcCode << "    goto instr_" << (funcCompiler.ip + offset) << ";\n";
                break;
            }
            case OpCode::OP_JUMP_IF_FALSE: {
                uint16_t offset = funcCompiler.readShort();
                funcCode << "    if (!locals[0].as.number) goto instr_" << (funcCompiler.ip + offset) << ";\n";
                break;
            }
            case OpCode::OP_LOOP: {
                uint16_t offset = funcCompiler.readShort();
                funcCode << "    goto instr_" << (funcCompiler.ip - offset) << ";\n";
                break;
            }
            case OpCode::OP_LOOP_IF_LESS_LOCAL: {
                uint8_t slot = funcCompiler.readByte();
                uint8_t constIdx = funcCompiler.readByte();  // Constant index (was missing!)
                uint16_t offset = funcCompiler.readShort();
                funcCode << "    if (!(locals[" << static_cast<int>(slot) << "].as.number < constants[" << static_cast<int>(constIdx) << "].as.number)) goto instr_" << (funcCompiler.ip + offset) << ";\n";
                break;
            }
            case OpCode::OP_CALL: {
                uint8_t argCount = funcCompiler.readByte();
                funcCode << "    // CALL (interpreter fallback)\n";
                break;
            }
            default:
                break;
        }
    }
    
    funcCode << "    return locals[0];\n";
    funcCode << "}\n\n";
    
    return funcCode.str();
}

} // namespace aot
} // namespace neutron
