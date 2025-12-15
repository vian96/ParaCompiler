#pragma once

#include <iostream>

#include "ast.hpp"
#include "visitor.hpp"

namespace ParaCompiler::Visitor {

class DumpVisitor : public Visitor {
    int indent_level = 0;

    void print_indent() const {
        for (int i = 0; i < indent_level; ++i) std::cout << "  |";

        if (indent_level > 0) std::cout << "-- ";
    }

   public:
    void visit(AST::Program &p) override {
        print_indent();
        std::cout << "Program\n";

        indent_level++;
        for (const auto &stmt : p.statements)
            if (stmt) stmt->accept(*this);

        indent_level--;
    }

    // --- Statements ---
    void visit(AST::Assignment &node) override {
        print_indent();
        std::cout << "Assignment [Name: " << node.name << "]\n";

        indent_level++;
        if (node.typeSpec) node.typeSpec->accept(*this);
        if (node.val) node.val->accept(*this);
        indent_level--;
    }

    void visit(AST::Print &node) override {
        print_indent();
        std::cout << "Output\n";

        indent_level++;
        if (node.expr) node.expr->accept(*this);
        indent_level--;
    }

    void visit(AST::ExprStmt &node) override {
        print_indent();
        std::cout << "ExprStmt\n";

        indent_level++;
        if (node.expr) node.expr->accept(*this);
        indent_level--;
    }

    // --- Expressions & Leaves ---
    void visit(AST::BinExpr &node) override {
        print_indent();
        std::cout << "BinExpr [" << node.op << "]\n";

        indent_level++;
        node.left->accept(*this);
        node.right->accept(*this);
        indent_level--;
    }

    void visit(AST::UnaryExpr &node) override {
        print_indent();
        std::cout << "UnaryExpr [" << node.op << "]\n";

        indent_level++;
        node.expr->accept(*this);
        indent_level--;
    }

    void visit(AST::IntLit &node) override {
        print_indent();
        std::cout << "IntLit [" << node.val << "]\n";
    }

    void visit(AST::Id &node) override {
        print_indent();
        std::cout << "Id [" << node.val << "]\n";
    }

    void visit(AST::Input &) override {
        print_indent();
        std::cout << "InputExpr\n";
    }

    void visit(AST::TypeSpec &node) override {
        print_indent();
        std::cout << "TypeSpec [" << node.name << "]\n";
    }

    // --- Unused/Abstract ---
    // These shouldn't be called directly in a valid AST,
    // but must be implemented.
    void visit(AST::Node &) override {}
    void visit(AST::Statement &) override {}
    void visit(AST::Expr &) override {}
};

}  // namespace ParaCompiler::Visitor
