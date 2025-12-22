module;
#include <memory>
#include <string>
#include <utility>
#include <vector>

export module ParaCompiler:AST;

import :Visitor;
import :Types;

export namespace ParaCompiler::Symbols {
struct Symbol;
}

export namespace ParaCompiler::AST {

struct Node {
    virtual void accept(Visitor::Visitor &visitor) = 0;
    virtual ~Node() = default;
};

#define PARACOMPILER_AST_OVERRIDE_ACCEPT \
    void accept(Visitor::Visitor &v) override { v.visit(*this); }

struct TypeSpec : Node {
    std::string name;
    bool is_int = true;
    size_t int_width = 32;

    bool is_func = false;
    std::vector<std::pair<std::string, std::unique_ptr<TypeSpec>>> args;

    PARACOMPILER_AST_OVERRIDE_ACCEPT
};

struct Statement : Node {};

struct Program : Node {
    std::vector<std::unique_ptr<Statement>> statements;
    PARACOMPILER_AST_OVERRIDE_ACCEPT
};

struct Expr : Node {
    virtual bool is_lvalue() const { return false; }
    const Types::Type *type = nullptr;

    PARACOMPILER_AST_OVERRIDE_ACCEPT
};

// --- STATEMENTS ---

struct Block : Node {
    std::vector<std::unique_ptr<Statement>> statements;
    PARACOMPILER_AST_OVERRIDE_ACCEPT
};

struct Assignment : Statement {
    std::string name;
    std::unique_ptr<TypeSpec> typeSpec = nullptr;
    std::unique_ptr<Expr> left = nullptr;
    std::unique_ptr<Expr> val = nullptr;

    PARACOMPILER_AST_OVERRIDE_ACCEPT
};

struct Print : Statement {
    std::unique_ptr<Expr> expr;
    PARACOMPILER_AST_OVERRIDE_ACCEPT
};

struct RetStmt : Statement {
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
    std::string op;  // possible are +,-,*,/,&&,||,<,>,==,<=,>=
    std::unique_ptr<Expr> left;
    std::unique_ptr<Expr> right;
    PARACOMPILER_AST_OVERRIDE_ACCEPT
};

struct IntLit : Expr {
    // TODO: compile time constants are max parsed 64 bits
    int64_t val;
    PARACOMPILER_AST_OVERRIDE_ACCEPT
};

struct Id : Expr {
    bool is_lvalue() const override { return true; }
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
    Symbols::Symbol *i_sym;

    PARACOMPILER_AST_OVERRIDE_ACCEPT
};

struct Conversion : Expr {
    std::unique_ptr<Expr> expr;

    Conversion(std::unique_ptr<Expr> expr_, const Types::Type *res_type_)
        : expr(std::move(expr_)) {
        type = res_type_;
    }

    PARACOMPILER_AST_OVERRIDE_ACCEPT
};

struct GlueEntry {
    std::string name;
    std::unique_ptr<Expr> val;
};

struct Glue : Expr {
    // because it creates a temporary alloca and uses it
    bool is_lvalue() const override { return true; }
    std::vector<GlueEntry> vals;
    PARACOMPILER_AST_OVERRIDE_ACCEPT
};

struct DotExpr : Expr {
    bool is_lvalue() const override { return true; }
    std::unique_ptr<Expr> left;
    std::string id;
    size_t field_ind;
    PARACOMPILER_AST_OVERRIDE_ACCEPT
};

struct LValToRVal : Expr {
    std::unique_ptr<Expr> expr;
    PARACOMPILER_AST_OVERRIDE_ACCEPT
};

struct IndexExpr : Expr {
    bool is_lvalue() const override { return true; }
    std::unique_ptr<Expr> left;
    size_t ind;
    PARACOMPILER_AST_OVERRIDE_ACCEPT
};

struct FuncBody : Expr {
    std::unique_ptr<Block> body;

    PARACOMPILER_AST_OVERRIDE_ACCEPT
};

#undef PARACOMPILER_AST_OVERRIDE_ACCEPT
};  // namespace ParaCompiler::AST
