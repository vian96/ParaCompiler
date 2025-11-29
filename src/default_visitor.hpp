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

    void visit(AST::VarDecl &node) override {
        if (node.val) node.val->accept(*this);
    }

    // we don't visit abstract nodes directly in the AST,
    // but if we do, just do nothing or cast.
    void visit(AST::Node &) override {}
    void visit(AST::Expr &) override {}
    void visit(AST::Statement &) override {}
    void visit(AST::TypeSpec &) override {}

    void visit(AST::IntLit &) override {}
    void visit(AST::Id &) override {}
    void visit(AST::Input &) override {}
};

}  // namespace ParaCompiler::Visitor
