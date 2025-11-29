#pragma once

namespace ParaCompiler::AST {
struct Node;
struct Statement;
struct Program;
struct Expr;
struct VarDecl;
struct IntLit;
struct Id;
struct Input;
struct TypeSpec;
struct Assignment;
struct Print;
struct ExprStmt;
}  // namespace ParaCompiler::AST

namespace ParaCompiler::Visitor {

struct Visitor {
    Visitor() = default;
    virtual ~Visitor() = default;

    virtual void visit(AST::Node &) = 0;
    virtual void visit(AST::Statement &) = 0;
    virtual void visit(AST::Program &) = 0;
    virtual void visit(AST::TypeSpec &) = 0;

    virtual void visit(AST::Expr &) = 0;
    virtual void visit(AST::VarDecl &) = 0;
    virtual void visit(AST::IntLit &) = 0;
    virtual void visit(AST::Id &) = 0;
    virtual void visit(AST::Input &) = 0;

    virtual void visit(AST::Assignment &) = 0;
    virtual void visit(AST::Print &) = 0;
    virtual void visit(AST::ExprStmt &) = 0;
};

}  // namespace ParaCompiler::Visitor
