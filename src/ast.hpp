#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "visitor.hpp"

namespace ParaCompiler::Symbols {
struct Symbol;
}

namespace ParaCompiler::AST {

struct Node {
    virtual void accept(Visitor::Visitor &visitor) = 0;
    virtual ~Node() = default;
};

#define PARACOMPILER_AST_OVERRIDE_ACCEPT \
    void accept(Visitor::Visitor &v) override { v.visit(*this); }

struct TypeSpec : Node {
    std::string name;
    void *info = nullptr;
    PARACOMPILER_AST_OVERRIDE_ACCEPT
};

struct Statement : Node {};

struct Program : Node {
    std::vector<std::unique_ptr<Statement>> statements;
    PARACOMPILER_AST_OVERRIDE_ACCEPT
};

struct Expr : Node {
    void *type = nullptr;
    PARACOMPILER_AST_OVERRIDE_ACCEPT
};

// --- STATEMENTS ---

struct Block : Node {
    std::vector<std::unique_ptr<Statement>> statements;
    PARACOMPILER_AST_OVERRIDE_ACCEPT
};

struct Assignment : Statement {
    std::string name;
    std::unique_ptr<TypeSpec> typeSpec;
    std::unique_ptr<Expr> val;
    Symbols::Symbol *sym;

    PARACOMPILER_AST_OVERRIDE_ACCEPT
};

struct Print : Statement {
    std::unique_ptr<Expr> expr;
    PARACOMPILER_AST_OVERRIDE_ACCEPT
};

struct ExprStmt : Statement {
    std::unique_ptr<Expr> expr;
    PARACOMPILER_AST_OVERRIDE_ACCEPT
};

// --- EXPRESSIONS ---

struct UnaryExpr : Expr {
    char op;
    std::unique_ptr<Expr> expr;
    PARACOMPILER_AST_OVERRIDE_ACCEPT
};

struct BinExpr : Expr {
    std::string op;
    std::unique_ptr<Expr> left;
    std::unique_ptr<Expr> right;
    PARACOMPILER_AST_OVERRIDE_ACCEPT
};

struct IntLit : Expr {
    int64_t val;
    PARACOMPILER_AST_OVERRIDE_ACCEPT
};

struct Id : Expr {
    std::string val;
    Symbols::Symbol *sym;
    PARACOMPILER_AST_OVERRIDE_ACCEPT
};

struct Input : Expr {
    PARACOMPILER_AST_OVERRIDE_ACCEPT
};

struct IfStmt : Statement {
    std::unique_ptr<Expr> expr;
    std::unique_ptr<Block> trueb;
    std::unique_ptr<Block> falseb;

    PARACOMPILER_AST_OVERRIDE_ACCEPT
};

struct WhileStmt : Statement {
    std::unique_ptr<Expr> expr;
    std::unique_ptr<Block> body;

    PARACOMPILER_AST_OVERRIDE_ACCEPT
};

struct ForStmt : Statement {
    std::string id;
    std::unique_ptr<Id> container;
    std::vector<std::unique_ptr<Expr>> slice;
    std::unique_ptr<Block> body;

    PARACOMPILER_AST_OVERRIDE_ACCEPT
};

#undef PARACOMPILER_AST_OVERRIDE_ACCEPT
};  // namespace ParaCompiler::AST
