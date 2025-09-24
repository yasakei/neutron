#ifndef NEUTRON_COMPILER_H
#define NEUTRON_COMPILER_H

#include "bytecode.h"
#include "expr.h"
#include "vm.h"

namespace neutron {

struct Local {
    Token name;
    int depth;
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
    void visitExpressionStmt(const ExpressionStmt* stmt);
    void visitSayStmt(const SayStmt* stmt);
    void visitVarStmt(const VarStmt* stmt);
    void visitBlockStmt(const BlockStmt* stmt);
    void visitIfStmt(const IfStmt* stmt);
    void visitWhileStmt(const WhileStmt* stmt);
    void visitUseStmt(const UseStmt* stmt);
    void visitFunctionStmt(const FunctionStmt* stmt);
    void visitReturnStmt(const ReturnStmt* stmt);

    Compiler* enclosing;
    Function* function;
    VM& vm;
    Chunk* chunk; // Current chunk being compiled
    int scopeDepth;
    std::vector<Local> locals;
};
} // namespace neutron

#endif // NEUTRON_COMPILER_H
