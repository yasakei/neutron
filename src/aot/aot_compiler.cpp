/*
 * Neutron Programming Language - AOT Compiler Implementation
 * Copyright (c) 2026 yasakei
 */

#include "aot/aot_compiler.h"
#include "types/obj_string.h"
#include <iostream>
#include <iomanip>
#include <cmath>

namespace neutron {
namespace aot {

AotCompiler::AotCompiler(const Chunk* c) : chunk(c), ip(0) {}

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
    code << "#include <iostream>\n";
    code << "#include <cmath>\n";
    code << "#include <string>\n";
    code << "#include <cstdint>\n\n";
    
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
                uint16_t offset = (chunk->code[scanIp] << 8) | chunk->code[scanIp + 1];
                scanIp += 2;
                size_t target = (op == OpCode::OP_LOOP) ? (instrStart - offset) : (scanIp + offset);
                if (target <= codeSize) isJumpTarget[target] = true;
                break;
            }
            case OpCode::OP_LOOP_IF_LESS_LOCAL: {
                scanIp++; // slot
                uint16_t offset = (chunk->code[scanIp] << 8) | chunk->code[scanIp + 1];
                scanIp += 2;
                size_t target = instrStart - offset;
                if (target <= codeSize) isJumpTarget[target] = true;
                break;
            }
            default:
                // Skip operands
                if (op == OpCode::OP_CONSTANT || op == OpCode::OP_GET_LOCAL || 
                    op == OpCode::OP_SET_LOCAL || op == OpCode::OP_GET_UPVALUE ||
                    op == OpCode::OP_SET_UPVALUE || op == OpCode::OP_CLOSE_UPVALUE ||
                    op == OpCode::OP_GET_PROPERTY || op == OpCode::OP_SET_PROPERTY ||
                    op == OpCode::OP_DEFINE_GLOBAL || op == OpCode::OP_GET_GLOBAL ||
                    op == OpCode::OP_SET_GLOBAL) {
                    scanIp++;
                } else if (op == OpCode::OP_CONSTANT_LONG) {
                    scanIp += 2;
                } else if (op == OpCode::OP_CALL || op == OpCode::OP_CALL_FAST || op == OpCode::OP_ARRAY || op == OpCode::OP_INVOKE) {
                    scanIp++;
                } else if (op == OpCode::OP_CLOSURE) {
                    scanIp += 2;
                    // Skip upvalue info
                    uint16_t numUpvalues = (chunk->code[scanIp] << 8) | chunk->code[scanIp + 1];
                    scanIp += 2 + numUpvalues * 2;
                }
                break;
        }
    }
    isJumpTarget[0] = true; // Always need entry point

    // Second pass: generate code with labels only at jump targets
    ip = 0;
    while (ip < codeSize) {
        // Emit label if this is a jump target
        if (isJumpTarget[ip] && ip != 0) {
            code << "    instr_" << ip << ":;\n";
        }
        
        size_t instrStart = ip;
        uint8_t instruction = readByte();
        OpCode op = static_cast<OpCode>(instruction);
        
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
                code << "    // GET_LOCAL " << slot << "\n";
                code << "    stack[sp++] = locals[" << slot << "];\n\n";
                break;
            }
            
            case OpCode::OP_SET_LOCAL: {
                uint8_t slot = readByte();
                code << "    // SET_LOCAL " << slot << "\n";
                code << "    locals[" << slot << "] = stack[--sp];\n\n";
                break;
            }
            
            case OpCode::OP_GET_GLOBAL: {
                // For simplicity, globals not fully supported in AOT v1
                code << "    // GET_GLOBAL (not supported in AOT v1)\n";
                code << "    stack[sp++] = Value();\n\n";
                break;
            }
            
            case OpCode::OP_SET_GLOBAL:
                // Skip operand bytes
                readByte(); // Skip global name index
                code << "    // SET_GLOBAL (not supported in AOT v1)\n\n";
                break;
                
            case OpCode::OP_ADD:
                code << "    // ADD\n";
                code << "    { Value b = stack[--sp]; Value a = stack[--sp];\n";
                code << "      if (a.type == ValueType::NUMBER && b.type == ValueType::NUMBER)\n";
                code << "        stack[sp++] = Value(a.as.number + b.as.number);\n";
                code << "      else if (a.type == ValueType::OBJ_STRING || b.type == ValueType::OBJ_STRING)\n";
                code << "        { std::string s = a.toString() + b.toString(); stack[sp++] = Value(s.c_str()); }\n";
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
                code << "      bool truthy = (a.type == ValueType::BOOLEAN && a.as.boolean) || a.type == ValueType::NUMBER;\n";
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
                code << "    goto instr_" << (ip - offset) << ";\n\n";
                break;
            }
            
            case OpCode::OP_LOOP_IF_LESS_LOCAL: {
                uint8_t slot = readByte();
                uint16_t offset = readShort();
                code << "    // LOOP_IF_LESS_LOCAL slot=" << slot << " offset=" << offset << "\n";
                code << "    if (locals[" << slot << "].as.number < constants[" << (ip - 3) << "].as.number) goto instr_" << (ip - offset) << ";\n\n";
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
                code << "    // CALL " << static_cast<int>(argCount) << " args (not supported in AOT v1)\n";
                code << "    sp -= " << static_cast<int>(argCount) << ";\n\n";
                break;
            }

            case OpCode::OP_CALL_FAST: {
                uint8_t argCount = readByte();
                code << "    // CALL_FAST " << static_cast<int>(argCount) << " args (not supported in AOT v1)\n";
                code << "    sp -= " << static_cast<int>(argCount) << ";\n\n";
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
                
            case OpCode::OP_ARRAY:
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
                if (op == OpCode::OP_INVOKE || op == OpCode::OP_ARRAY) {
                    readByte(); // arg count or array size
                }
                break;
                
            case OpCode::OP_DEFINE_GLOBAL:
            case OpCode::OP_DEFINE_TYPED_GLOBAL:
            case OpCode::OP_SET_GLOBAL_TYPED:
            case OpCode::OP_SET_LOCAL_TYPED:
            case OpCode::OP_GET_GLOBAL_FAST:
            case OpCode::OP_SET_GLOBAL_FAST:
            case OpCode::OP_LOAD_LOCAL_0:
            case OpCode::OP_LOAD_LOCAL_1:
            case OpCode::OP_LOAD_LOCAL_2:
            case OpCode::OP_LOAD_LOCAL_3:
            case OpCode::OP_CONST_ZERO:
            case OpCode::OP_CONST_ONE:
            case OpCode::OP_CONST_INT8:
            case OpCode::OP_INC_LOCAL_INT:
            case OpCode::OP_DEC_LOCAL_INT:
            case OpCode::OP_ADD_LOCAL_CONST:
            case OpCode::OP_INCREMENT_LOCAL:
            case OpCode::OP_DECREMENT_LOCAL:
            case OpCode::OP_INCREMENT_GLOBAL:
            case OpCode::OP_TYPE_GUARD:
            case OpCode::OP_LOOP_HINT:
            case OpCode::OP_TAIL_CALL:
            case OpCode::OP_BREAK:
            case OpCode::OP_CONTINUE:
            case OpCode::OP_TRY:
            case OpCode::OP_END_TRY:
            case OpCode::OP_THROW:
            case OpCode::OP_LOGICAL_AND:
            case OpCode::OP_LOGICAL_OR:
            case OpCode::OP_VALIDATE_SAFE_FUNCTION:
            case OpCode::OP_VALIDATE_SAFE_VARIABLE:
            case OpCode::OP_VALIDATE_SAFE_FILE_FUNCTION:
            case OpCode::OP_VALIDATE_SAFE_FILE_VARIABLE:
                // Not implemented in AOT v1 - skip
                code << "    // " << static_cast<int>(op) << " (not supported in AOT v1)\n\n";
                // Skip operands as needed
                break;
                
            case OpCode::OP_COUNT:
                break;
        }
    }
}

std::string AotCompiler::generateCode(const std::string& functionName) {
    code.str("");
    code.clear();
    
    generatePrologue(functionName);
    generateBytecodeBody();
    generateEpilogue();
    
    return code.str();
}

} // namespace aot
} // namespace neutron
