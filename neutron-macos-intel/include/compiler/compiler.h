#ifndef NEUTRON_COMPILER_H
#define NEUTRON_COMPILER_H

#include "compiler/bytecode.h"
#include "expr.h"
#include "vm.h"
#include <optional>
#include <set>

/**
 * Represents a local variable in the current compilation scope.
 *
 * Holds the variable's name token, its scope depth, and an optional type annotation token.
 */

/**
 * Create a top-level Compiler bound to a VM.
 * @param vm VM instance used during compilation.
 * @param isSafeFile When true, treat the entire compilation unit as a safe file (.ntsc).
 */

/**
 * Create a nested Compiler that inherits scope and state from an enclosing compiler.
 * @param enclosing Pointer to the enclosing Compiler context.
 */

/**
 * Compile a list of statements into a Function.
 * @param statements Vector of statement pointers forming the top-level body.
 * @returns Pointer to the compiled Function.
 */

/**
 * Compile a function declaration with an explicitly provided body into a Function.
 * @param stmt The FunctionStmt AST node describing the function (name, params, etc.).
 * @param body Vector of statements comprising the function body.
 * @returns Pointer to the compiled Function.
 */

/**
 * Begin a new local scope, increasing scope depth and preparing local tracking.
 */

/**
 * End the current local scope, emitting any necessary cleanup for scoped locals.
 */

/**
 * Compile a single statement AST node.
 * @param stmt Pointer to the statement node to compile.
 */

/**
 * Compile a single expression AST node.
 * @param expr Pointer to the expression node to compile.
 */

/**
 * Emit a single byte into the current chunk.
 * @param byte Byte value to append.
 */

/**
 * Emit two bytes into the current chunk.
 * @param byte1 First byte to append.
 * @param byte2 Second byte to append.
 */

/**
 * Emit the return sequence for the current function.
 */

/**
 * Add a constant to the function's constant pool or retrieve its index.
 * @param value The Value to store as a constant.
 * @returns Index of the constant within the constant pool.
 */

/**
 * Emit bytecode that loads the given constant at runtime.
 * @param value The Value to emit as a loadable constant.
 */

/**
 * Emit a jump instruction with a placeholder offset to be patched later.
 * @param instruction Opcode for the jump instruction to emit.
 * @returns The byte offset where the jump's operand was written (for later patching).
 */

/**
 * Patch a previously emitted jump instruction to jump to the current bytecode position.
 * @param offset The byte offset returned by emitJump for the jump to patch.
 */

/**
 * Emit a loop back-jump referencing a loop start position.
 * @param loopStart Bytecode position marking the start of the loop.
 */

/**
 * Resolve a local variable by name within the current compilation context.
 * @param name Token representing the identifier to resolve.
 * @returns Local variable index if found, or -1 if not found.
 */

/**
 * Validate that an optional type annotation matches an actual runtime ValueType.
 * @param typeAnnotation Optional token representing the declared type.
 * @param actualType The runtime ValueType observed for the expression/value.
 * @returns `true` if the annotation is absent or matches `actualType`, `false` otherwise.
 */

/**
 * Determine the ValueType of a given expression AST node.
 * @param expr Pointer to the expression node.
 * @returns The resolved ValueType for the expression.
 */

/**
 * Convert a ValueType enum value to its string representation.
 * @param type The ValueType to convert.
 * @returns Human-readable string for `type`.
 */

/**
 * Convert a TokenType enum value to its string representation.
 * @param type The TokenType to convert.
 * @returns Human-readable string for `type`.
 */
namespace neutron {

struct Local {
    Token name;
    int depth;
    std::optional<Token> typeAnnotation;  // Optional type annotation for this local
};

class Compiler {
public:
    Compiler(VM& vm, bool isSafeFile = false);
    Compiler(Compiler* enclosing);
    Function* compile(const std::vector<std::unique_ptr<Stmt>>& statements);
    Function* compile(const FunctionStmt* stmt, const std::vector<std::unique_ptr<Stmt>>& body);

    void beginScope();
    void endScope();
    void compileStatement(const Stmt* stmt);
    void compileExpression(const Expr* expr);
    void emitByte(uint8_t byte);
    void emitBytes(uint8_t byte1, uint8_t byte2);
    void emitReturn();
    uint8_t makeConstant(const Value& value);
    void emitConstant(const Value& value);
    int emitJump(uint8_t instruction);
    void patchJump(int offset);
    void emitLoop(int loopStart);
    int resolveLocal(const Token& name);
    bool validateType(const std::optional<Token>& typeAnnotation, ValueType actualType);
    ValueType getExpressionType(const Expr* expr);
    std::string valueTypeToString(ValueType type);
    std::string tokenTypeToString(TokenType type);

public:
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

    Compiler* enclosing;
    Function* function;
    VM& vm;
    Chunk* chunk; // Current chunk being compiled
    int scopeDepth;
    int currentLine; // Current line number being compiled
    std::vector<Local> locals;
    
    // Loop information for break/continue
    std::vector<int> loopStarts;  // Stack of loop start positions
    std::vector<int> continueTargets;  // Stack of continue target positions (for for-loop increments)
    std::vector<std::vector<int>> breakJumps;  // Stack of break jump positions for each loop
    std::vector<std::vector<int>> continueJumps;  // Stack of continue jump positions for each loop
    
    // Track declared global variables to prevent duplicates
    std::set<std::string> declaredGlobals;
    
    // Track if we're currently in a safe block (enforces type annotations)
    bool inSafeBlock;
    
    // Track if we're compiling a safe file (.ntsc)
    bool isSafeFile;
};
} // namespace neutron

#endif // NEUTRON_COMPILER_H