#pragma once

#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

#include "visitor.hpp"

namespace ParaCompiler::AST {

struct Node {
    virtual void accept(Visitor::Visitor &visitor) = 0;
    virtual ~Node() = default;
};

#define PARACOMPILER_AST_OVERRIDE_ACCEPT \
    void accept(Visitor::Visitor &v) override { v.visit(*this); }

struct TypeSpec : Node {
    std::string_view name;
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

struct VarDecl : Statement {
    std::string_view name;
    std::unique_ptr<TypeSpec> typeSpec;
    std::unique_ptr<Expr> val;
    PARACOMPILER_AST_OVERRIDE_ACCEPT
};

struct Assignment : Statement {
    std::string_view name;
    std::unique_ptr<Expr> val;
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

struct IntLit : Expr {
    int64_t val;
    PARACOMPILER_AST_OVERRIDE_ACCEPT
};

struct Id : Expr {
    std::string_view val;
    PARACOMPILER_AST_OVERRIDE_ACCEPT
};

struct Input : Expr {
    PARACOMPILER_AST_OVERRIDE_ACCEPT
};

#undef PARACOMPILER_AST_OVERRIDE_ACCEPT
};  // namespace ParaCompiler::AST
