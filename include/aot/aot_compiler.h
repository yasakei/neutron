/*
 * Neutron Programming Language - AOT Compiler
 * Copyright (c) 2026 yasakei
 * 
 * Translates Neutron bytecode to native C++ code at compile time.
 */

#ifndef NEUTRON_AOT_COMPILER_H
#define NEUTRON_AOT_COMPILER_H

#include "compiler/bytecode.h"
#include "types/value.h"
#include <string>
#include <sstream>
#include <vector>
#include <unordered_map>

namespace neutron {
namespace aot {

// Target platform for cross-compilation
enum class TargetPlatform {
    NATIVE,      // Current platform
    LINUX_X64,
    LINUX_ARM64,
    MACOS_X64,
    MACOS_ARM64,
    WINDOWS_X64,
    WINDOWS_X86
};

class AotCompiler {
public:
    AotCompiler(const Chunk* chunk);
    
    // Enable debug symbol generation
    void setDebugMode(bool enabled) { generateDebugSymbols = enabled; }
    
    // Set target platform for cross-compilation
    void setTargetPlatform(TargetPlatform platform) { targetPlatform = platform; }
    TargetPlatform getTargetPlatform() const { return targetPlatform; }

    // Generate native C++ code from bytecode
    std::string generateCode(const std::string& functionName = "main");
    
    // Get source mapping (bytecode offset -> C++ line)
    const std::unordered_map<size_t, size_t>& getSourceMap() const { return sourceMap; }

private:
    const Chunk* chunk;
    std::ostringstream code;
    size_t ip;  // Instruction pointer during code generation
    
    // Global variable tracking
    std::vector<std::string> globalNames;  // Names of defined globals
    std::unordered_map<std::string, int> globalSlotMap;  // Name -> slot index
    
    // Optimization: constant folding state
    std::unordered_map<int, double> knownConstantLocals;  // slot -> known constant value
    
    // Debug symbols
    bool generateDebugSymbols = false;
    std::unordered_map<size_t, size_t> sourceMap;  // bytecode offset -> C++ line number
    size_t currentCppLine;

    // Static array pool for AOT-compiled arrays
    std::vector<std::pair<std::string, size_t>> staticArrays;  // (name, size)
    int arrayCounter = 0;
    
    // Cross-compilation target
    TargetPlatform targetPlatform = TargetPlatform::NATIVE;
    
    // Function code cache for nested function calls
    std::unordered_map<size_t, std::string> generatedFunctions;  // constant index -> C++ code

    // Code generation helpers
    void generatePrologue(const std::string& functionName);
    void generateEpilogue();
    void generateBytecodeBody();
    
    // First pass: collect global variable definitions
    void collectGlobalDefinitions();
    
    // Generate C++ function from chunk
    std::string generateFunctionCode(const Chunk* funcChunk, const std::string& funcName, int paramCount);
    
    // Optimization passes
    void optimizeBytecode(std::vector<uint8_t>& code, std::vector<Value>& constants);
    
    // Debug helpers
    void emitDebugComment(const std::string& comment);
    void recordSourceLocation(size_t bytecodeOffset);

    // Instruction generators
    void emitConstant();
    void emitReturn();
    void emitArithmetic(OpCode op);
    void emitComparison(OpCode op);
    void emitLogic();
    void emitStackOps();
    void emitLocalVars();
    void emitGlobalVars();
    void emitJump();
    void emitCall();
    void emitSay();

    // Utility
    std::string constantToCpp(size_t index);
    std::string valueTypeToString(ValueType type);
    uint8_t readByte();
    uint16_t readShort();
};

} // namespace aot
} // namespace neutron

#endif // NEUTRON_AOT_COMPILER_H
