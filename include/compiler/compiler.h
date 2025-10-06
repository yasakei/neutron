#ifndef NEUTRON_COMPILER_H
#define NEUTRON_COMPILER_H

#include "compiler/bytecode.h"
#include "expr.h"
#include "vm.h"
#include <optional>

namespace neutron {

struct Local {
    Token name;
    int depth;
    std::optional<Token> typeAnnotation;  // Optional type annotation for this local
};

class Compiler {
public:
    Compiler(VM& vm);
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
    void visitExpressionStmt(const ExpressionStmt* stmt);
    void visitSayStmt(const SayStmt* stmt);
    void visitVarStmt(const VarStmt* stmt);
    void visitBlockStmt(const BlockStmt* stmt);
    void visitIfStmt(const IfStmt* stmt);
    void visitWhileStmt(const WhileStmt* stmt);
    void visitClassStmt(const ClassStmt* stmt);
    void visitUseStmt(const UseStmt* stmt);
    void visitFunctionStmt(const FunctionStmt* stmt);
    void visitReturnStmt(const ReturnStmt* stmt);
    void visitBreakStmt(const BreakStmt* stmt);
    void visitContinueStmt(const ContinueStmt* stmt);
    void visitMatchStmt(const MatchStmt* stmt);
    void visitTryStmt(const TryStmt* stmt);
    void visitThrowStmt(const ThrowStmt* stmt);

    Compiler* enclosing;
    Function* function;
    VM& vm;
    Chunk* chunk; // Current chunk being compiled
    int scopeDepth;
    std::vector<Local> locals;
    
    // Loop information for break/continue
    std::vector<int> loopStarts;  // Stack of loop start positions
    std::vector<int> continueTargets;  // Stack of continue target positions (for for-loop increments)
    std::vector<std::vector<int>> breakJumps;  // Stack of break jump positions for each loop
    std::vector<std::vector<int>> continueJumps;  // Stack of continue jump positions for each loop
};
} // namespace neutron

#endif // NEUTRON_COMPILER_H
