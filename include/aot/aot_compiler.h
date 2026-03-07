/*
 * Neutron Programming Language - AOT Compiler
 * Copyright (c) 2026 yasakei
 *
 * This software is distributed under the Neutron Permissive License (NPL) 1.1.
 * For full license text, see LICENSE file in the root directory.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/*
 * Code Documentation: AOT Compiler (aot_compiler.h)
 * =================================================
 * 
 * This header defines the AotCompiler class - the Ahead-Of-Time compiler
 * that translates Neutron bytecode to native C++ code for standalone
 * executable generation.
 * 
 * What This File Includes:
 * ------------------------
 * - AotCompiler class: Bytecode to C++ transpiler
 * - TargetPlatform enum: Cross-compilation targets
 * - Code generation: Prologue, epilogue, instruction emitters
 * - Optimization: Constant folding, bytecode optimization
 * - Debug symbols: Source mapping for debugging
 * 
 * How It Works:
 * -------------
 * The AOT compiler performs ahead-of-time compilation:
 * 
 * 1. Bytecode Analysis: Parse bytecode chunk and constants
 * 2. Global Collection: Identify all global variables
 * 3. Optimization: Apply constant folding and peephole optimization
 * 4. Code Generation: Emit equivalent C++ code
 * 5. Source Mapping: Generate debug symbol information
 * 
 * The generated C++ code:
 * - Uses the Neutron runtime (vm.cpp, value.cpp, etc.)
 * - Can be compiled with any C++17 compiler
 * - Produces standalone executables (no interpreter needed)
 * 
 * Supported Targets:
 * - NATIVE: Current platform (default)
 * - LINUX_X64, LINUX_ARM64: Linux targets
 * - MACOS_X64, MACOS_ARM64: macOS targets  
 * - WINDOWS_X64, WINDOWS_X86: Windows targets
 * 
 * Adding Features:
 * ----------------
 * - New optimizations: Add optimization passes before code generation
 * - New targets: Extend TargetPlatform, add platform-specific code
 * - Debug formats: Add DWARF/PDB generation for better debugging
 * - Link-time optimization: Add LTO support for better performance
 * 
 * What You Should NOT Do:
 * -----------------------
 * - Do NOT compile unverified bytecode (security risk)
 * - Do NOT skip optimization passes (produces inefficient code)
 * - Do NOT ignore platform-specific ABI requirements
 * - Do NOT generate code without proper error handling
 * 
 * Performance Notes:
 * ------------------
 * - AOT compilation is slower than JIT but produces faster executables
 * - Generated code size is larger than JIT (includes runtime)
 * - Best for distribution, embedded systems, and performance-critical apps
 * 
 * Example Usage:
 * --------------
 * @code
 * // Compile bytecode to C++
 * AotCompiler compiler(chunk);
 * compiler.setTargetPlatform(TargetPlatform::NATIVE);
 * compiler.setDebugMode(true);
 * 
 * std::string cppCode = compiler.generateCode("main");
 * 
 * // Write to file and compile with g++
 * std::ofstream out("output.cpp");
 * out << cppCode;
 * out.close();
 * 
 * system("g++ -std=c++17 output.cpp -o program");
 * @endcode
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

/**
 * @brief TargetPlatform - Cross-compilation target specification.
 * 
 * Determines the target platform for generated code.
 * Affects calling conventions, binary format, and system calls.
 */
enum class TargetPlatform {
    NATIVE,       ///< Current platform (auto-detected)
    LINUX_X64,    ///< Linux x86_64
    LINUX_ARM64,  ///< Linux ARM64 (Raspberry Pi, etc.)
    MACOS_X64,    ///< macOS Intel
    MACOS_ARM64,  ///< macOS Apple Silicon (M1/M2)
    WINDOWS_X64,  ///< Windows x86_64
    WINDOWS_X86   ///< Windows x86 (legacy)
};

/**
 * @brief AotCompiler - Ahead-Of-Time Bytecode to C++ Compiler.
 * 
 * Translates Neutron bytecode into standalone C++ code that can be
 * compiled with any C++17 compiler. The generated code includes
 * the Neutron runtime and executes without an interpreter.
 * 
 * Compilation Process:
 * 1. collectGlobalDefinitions(): Scan for global variables
 * 2. optimizeBytecode(): Apply constant folding and peephole opts
 * 3. generatePrologue(): Emit includes, namespace, globals
 * 4. generateBytecodeBody(): Translate each instruction
 * 5. generateEpilogue(): Emit main() and cleanup
 * 
 * Optimization Features:
 * - Constant folding: Pre-compute constant expressions
 * - Dead code elimination: Remove unreachable code
 * - Inline expansion: Inline small functions
 * - Static array pool: Pre-allocate known arrays
 * 
 * Debug Features:
 * - Source mapping: Bytecode offset → C++ line number
 * - Debug comments: Annotate generated code with bytecode info
 * - Symbol preservation: Keep function names for debugging
 */
class AotCompiler {
public:
    /**
     * @brief Construct an AOT compiler for a bytecode chunk.
     * @param chunk The bytecode chunk to compile.
     */
    AotCompiler(const Chunk* chunk);

    /**
     * @brief Enable debug symbol generation.
     * @param enabled True to generate debug symbols.
     * 
     * When enabled, generates source mapping and debug comments
     * in the output C++ code.
     */
    void setDebugMode(bool enabled) { generateDebugSymbols = enabled; }

    /**
     * @brief Set target platform for cross-compilation.
     * @param platform The target platform.
     */
    void setTargetPlatform(TargetPlatform platform) { targetPlatform = platform; }
    
    /**
     * @brief Get the current target platform.
     * @return The target platform.
     */
    TargetPlatform getTargetPlatform() const { return targetPlatform; }

    /**
     * @brief Generate native C++ code from bytecode.
     * @param functionName Name for the generated function (default: "main").
     * @return Generated C++ source code.
     * 
     * The generated code includes:
     * - All necessary includes
     * - Global variable declarations
     * - Function definitions
     * - The main entry point
     */
    std::string generateCode(const std::string& functionName = "main");

    /**
     * @brief Get source mapping (bytecode offset → C++ line).
     * @return Map of bytecode offsets to C++ line numbers.
     * 
     * Used for debugging and error reporting in compiled executables.
     */
    const std::unordered_map<size_t, size_t>& getSourceMap() const { return sourceMap; }

private:
    const Chunk* chunk;           ///< Bytecode chunk being compiled
    std::ostringstream code;      ///< Output code stream
    size_t ip;                    ///< Instruction pointer during code generation

    // Global variable tracking
    std::vector<std::string> globalNames;         ///< Names of defined globals
    std::unordered_map<std::string, int> globalSlotMap;  ///< Name → slot index

    // Optimization: constant folding state
    std::unordered_map<int, double> knownConstantLocals;  ///< slot → known constant value

    // Debug symbols
    bool generateDebugSymbols = false;  ///< Enable debug symbol generation
    std::unordered_map<size_t, size_t> sourceMap;  ///< bytecode offset → C++ line number
    size_t currentCppLine;          ///< Current line in generated code

    // Static array pool for AOT-compiled arrays
    std::vector<std::pair<std::string, size_t>> staticArrays;  ///< (name, size)
    int arrayCounter;             ///< Counter for unique array names

    // Cross-compilation target
    TargetPlatform targetPlatform;  ///< Target platform

    // Function code cache for nested function calls
    std::unordered_map<size_t, std::string> generatedFunctions;  ///< constant index → C++ code

    // Code generation helpers
    void generatePrologue(const std::string& functionName);   ///< Emit includes, namespace, globals
    void generateEpilogue();                                   ///< Emit main() and cleanup
    void generateBytecodeBody();                               ///< Translate bytecode instructions

    // First pass: collect global variable definitions
    void collectGlobalDefinitions();  ///< Scan bytecode for global variable definitions

    // Generate C++ function from chunk
    std::string generateFunctionCode(const Chunk* funcChunk, const std::string& funcName, int paramCount);

    // Optimization passes
    void optimizeBytecode(std::vector<uint8_t>& code, std::vector<Value>& constants);

    // Debug helpers
    void emitDebugComment(const std::string& comment);  ///< Add comment to generated code
    void recordSourceLocation(size_t bytecodeOffset);   ///< Record source mapping

    // Instruction generators (emit C++ code for each opcode)
    void emitConstant();       ///< OP_CONSTANT
    void emitReturn();         ///< OP_RETURN
    void emitArithmetic(OpCode op);  ///< OP_ADD, OP_SUBTRACT, etc.
    void emitComparison(OpCode op);  ///< OP_EQUAL, OP_LESS, etc.
    void emitLogic();          ///< OP_NOT, OP_LOGICAL_AND, etc.
    void emitStackOps();       ///< OP_POP, OP_DUP
    void emitLocalVars();      ///< OP_GET_LOCAL, OP_SET_LOCAL
    void emitGlobalVars();     ///< OP_GET_GLOBAL, OP_SET_GLOBAL
    void emitJump();           ///< OP_JUMP, OP_JUMP_IF_FALSE
    void emitCall();           ///< OP_CALL, OP_INVOKE
    void emitSay();            ///< OP_SAY

    // Utility
    std::string constantToCpp(size_t index);     ///< Convert constant index to C++ code
    std::string valueTypeToString(ValueType type);  ///< Convert ValueType to string
    uint8_t readByte();       ///< Read next byte from bytecode
    uint16_t readShort();     ///< Read next 16-bit value from bytecode
};

} // namespace aot
} // namespace neutron

#endif // NEUTRON_AOT_COMPILER_H
