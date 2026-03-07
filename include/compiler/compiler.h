#ifndef NEUTRON_COMPILER_H
#define NEUTRON_COMPILER_H

/*
 * Code Documentation: Bytecode Compiler (compiler.h)
 * ==================================================
 * 
 * This header defines the Compiler class - the third phase of the compiler
 * pipeline. It transforms the AST into bytecode for the VM to execute.
 * 
 * What This File Includes:
 * ------------------------
 * - Compiler class: AST to bytecode compiler
 * - Local variable management: Symbol table and scope tracking
 * - Bytecode emission: Opcode generation for all AST nodes
 * - Type validation: Static type checking for safe blocks
 * 
 * How It Works:
 * -------------
 * The compiler uses the Visitor pattern to traverse the AST:
 * 1. Visit each statement/expression node
 * 2. Emit corresponding bytecode instructions
 * 3. Track local variables in a symbol table
 * 4. Resolve variable references (local vs global)
 * 5. Generate bytecode offsets for control flow
 * 
 * Variable Resolution:
 * - Locals are stored in stack slots, accessed by depth and index
 * - Globals are stored in the VM's global map, accessed by name
 * - Upvalues (captured locals) are tracked for closures
 * 
 * Adding Features:
 * ----------------
 * - New opcodes: Add to bytecode.h, implement visit method here
 * - New statements: Add visitor method, emit appropriate bytecode
 * - Type checking: Extend validateType() for new type rules
 * 
 * What You Should NOT Do:
 * -----------------------
 * - Do NOT emit invalid bytecode sequences (ensure stack balance)
 * - Do NOT forget to begin/end scopes for blocks
 * - Do NOT skip type validation in safe blocks
 * - Do NOT modify the AST during compilation (read-only)
 * 
 * Bytecode Generation Patterns:
 * -----------------------------
 * - Literals: OP_CONSTANT
 * - Variables: OP_GET_LOCAL / OP_GET_GLOBAL
 * - Assignment: OP_SET_LOCAL / OP_SET_GLOBAL
 * - Arithmetic: Emit operands, then OP_ADD/OP_SUBTRACT/etc.
 * - Control flow: OP_JUMP, OP_JUMP_IF_FALSE, OP_LOOP
 * - Functions: OP_CLOSURE for capturing upvalues
 */

// Windows macro undefs - must be before any standard library includes
// Because Windows thinks it knows better than the C++ standard
#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <windows.h>
    // Undefine Windows macros that conflict with C++ code
    // NOTE: Do NOT undefine FAR, NEAR, IN, OUT as they are needed by Windows headers
    #undef TRUE
    #undef FALSE
    #undef DELETE
    #undef ERROR
    #undef OPTIONAL
    #undef interface
    #undef small
    #undef max
    #undef min
#endif

#include "compiler/bytecode.h"
#include "expr.h"
#include "vm.h"
#include <optional>
#include <set>

namespace neutron {

/**
 * @brief Local - Symbol table entry for local variables.
 * 
 * Tracks local variables during compilation:
 * - name: Token containing the variable name
 * - depth: Lexical scope depth (0 = global/module scope)
 * - typeAnnotation: Optional type annotation for static checking
 */
struct Local {
    Token name;
    int depth;
    std::optional<Token> typeAnnotation;  ///< Optional type annotation for this local
};

/**
 * @brief Compiler - The Bytecode Generator: AST to bytecode translation.
 * 
 * The Compiler is the third stage of the compiler pipeline. It visits
 * AST nodes and emits corresponding bytecode instructions for the VM.
 * 
 * Key Responsibilities:
 * - Bytecode emission for all statement and expression types
 * - Local variable management with stack slot allocation
 * - Scope tracking for block-structured languages
 * - Type validation in safe blocks (type-annotated code)
 * - Control flow resolution (jump offsets, loop targets)
 * 
 * Compilation Process:
 * 1. Initialize compiler with empty chunk
 * 2. Visit each statement in the program
 * 3. Emit bytecode for each node
 * 4. Resolve forward references (patches jumps)
 * 5. Return compiled Function object
 * 
 * Example Bytecode:
 * @code
 * Source: var x = 1 + 2;
 * Bytecode:
 *   OP_CONSTANT 1.0
 *   OP_CONSTANT 2.0
 *   OP_ADD
 *   OP_DEFINE_GLOBAL "x"
 * @endcode
 */
class Compiler {
public:
    /**
     * @brief Construct a top-level compiler.
     * @param vm The VM instance for memory allocation.
     * @param isSafeFile Whether this is a safe file (.ntsc).
     */
    Compiler(VM& vm, bool isSafeFile = false);
    
    /**
     * @brief Construct a nested compiler (for closures).
     * @param enclosing The enclosing compiler for upvalue resolution.
     */
    Compiler(Compiler* enclosing);
    
    /**
     * @brief Compile a program (module-level statements).
     * @param statements The AST to compile.
     * @return Compiled Function object.
     */
    Function* compile(const std::vector<std::unique_ptr<Stmt>>& statements);
    
    /**
     * @brief Compile a function definition.
     * @param stmt The function statement.
     * @param body The function body statements.
     * @return Compiled Function object.
     */
    Function* compile(const FunctionStmt* stmt, const std::vector<std::unique_ptr<Stmt>>& body);

    // Scope management
    void beginScope();   ///< Enter a new lexical scope
    void endScope();     ///< Exit current scope, pop locals

    // Compilation entry points
    void compileStatement(const Stmt* stmt);   ///< Compile a statement
    void compileExpression(const Expr* expr);  ///< Compile an expression

    // Bytecode emission
    void emitByte(uint8_t byte);               ///< Emit a single byte
    void emitBytes(uint8_t byte1, uint8_t byte2);  ///< Emit two bytes
    void emitReturn();                         ///< Emit return instruction
    uint16_t makeConstant(const Value& value); ///< Add constant to pool, return index
    void emitConstant(const Value& value);     ///< Emit OP_CONSTANT with index
    int emitJump(uint8_t instruction);         ///< Emit jump, return offset to patch
    void patchJump(int offset);                ///< Patch jump offset after target is known
    void emitLoop(int loopStart);              ///< Emit loop back-edge
    int resolveLocal(const Token& name);       ///< Resolve variable to local slot

    // Type validation
    bool validateType(const std::optional<Token>& typeAnnotation, ValueType actualType);
    ValueType getExpressionType(const Expr* expr);
    std::string valueTypeToString(ValueType type);
    std::string tokenTypeToString(TokenType type);

public:
    // Visitor methods for expressions - each visits a specific AST node type
    void visitLiteralExpr(const LiteralExpr* expr);
    void visitVariableExpr(const VariableExpr* expr);
    void visitBinaryExpr(const BinaryExpr* expr);
    void visitUnaryExpr(const UnaryExpr* expr);
    void visitGroupingExpr(const GroupingExpr* expr);
    void visitMemberExpr(const MemberExpr* expr);
    void visitCallExpr(const CallExpr* expr);
    void visitAssignExpr(const AssignExpr* expr);
    void visitObjectExpr(const ObjectExpr* expr);
    void visitArrayExpr(const ArrayExpr* expr);
    void visitIndexGetExpr(const IndexGetExpr* expr);
    void visitIndexSetExpr(const IndexSetExpr* expr);
    void visitMemberSetExpr(const MemberSetExpr* expr);
    void visitThisExpr(const ThisExpr* expr);
    void visitFunctionExpr(const FunctionExpr* expr);
    void visitTernaryExpr(const TernaryExpr* expr);

    // Visitor methods for statements
    void visitExpressionStmt(const ExpressionStmt* stmt);
    void visitSayStmt(const SayStmt* stmt);
    void visitVarStmt(const VarStmt* stmt);
    void visitBlockStmt(const BlockStmt* stmt);
    void visitIfStmt(const IfStmt* stmt);
    void visitWhileStmt(const WhileStmt* stmt);
    void visitDoWhileStmt(const DoWhileStmt* stmt);
    void visitClassStmt(const ClassStmt* stmt);
    void visitUseStmt(const UseStmt* stmt);
    void visitFunctionStmt(const FunctionStmt* stmt);
    void visitReturnStmt(const ReturnStmt* stmt);
    void visitBreakStmt(const BreakStmt* stmt);
    void visitContinueStmt(const ContinueStmt* stmt);
    void visitMatchStmt(const MatchStmt* stmt);
    void visitTryStmt(const TryStmt* stmt);
    void visitThrowStmt(const ThrowStmt* stmt);
    void visitRetryStmt(const RetryStmt* stmt);
    void visitSafeStmt(const SafeStmt* stmt);

    // Compiler state
    Compiler* enclosing;      ///< Enclosing compiler (for closures)
    Function* function;       ///< Function being compiled
    VM& vm;                   ///< VM for memory allocation
    Chunk* chunk;             ///< Current bytecode chunk
    int scopeDepth;           ///< Current lexical scope depth
    int currentLine;          ///< Current source line for debugging
    std::vector<Local> locals;  ///< Local variable symbol table

    // Loop control - stacks for nested loops
    std::vector<int> loopStarts;  ///< Stack of loop start positions
    std::vector<int> continueTargets;  ///< Stack of continue targets (for-loop increments)
    std::vector<std::vector<int>> breakJumps;  ///< Stack of break jump positions per loop
    std::vector<std::vector<int>> continueJumps;  ///< Stack of continue jump positions per loop

    // Track declared global variables to prevent duplicates
    std::set<std::string> declaredGlobals;

    // Track if we're currently in a safe block (enforces type annotations)
    bool inSafeBlock;

    // Track if we're compiling a safe file (.ntsc)
    bool isSafeFile;
};
} // namespace neutron

#endif // NEUTRON_COMPILER_H
