#pragma once

#include "ast.hpp"
#include "visitor.hpp"

namespace ParaCompiler::Visitor {

class DefaultVisitor : public Visitor {
   public:
    void visit(AST::Program &p) override {
        for (const auto &stmt : p.statements)
            if (stmt) stmt->accept(*this);
    }

    void visit(AST::Assignment &node) override {
        if (node.typeSpec) node.val->accept(*this);
        if (node.val) node.val->accept(*this);
    }

    void visit(AST::Print &node) override {
        if (node.expr) node.expr->accept(*this);
    }

    void visit(AST::ExprStmt &node) override {
        if (node.expr) node.expr->accept(*this);
    }

    void visit(AST::BinExpr &node) override {
        node.left->accept(*this);
        node.right->accept(*this);
    }

    void visit(AST::UnaryExpr &node) override {
        node.expr->accept(*this);
    }

    void visit(AST::Node &) override {}
    void visit(AST::Expr &) override {}
    void visit(AST::Statement &) override {}
    void visit(AST::TypeSpec &) override {}

    void visit(AST::IntLit &) override {}
    void visit(AST::Id &) override {}
    void visit(AST::Input &) override {}
};

}  // namespace ParaCompiler::Visitor
