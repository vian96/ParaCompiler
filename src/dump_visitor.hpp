#pragma once

#include <iostream>
#include <memory>

#include "ast.hpp"
#include "symbol.hpp"
#include "types.hpp"
#include "visitor.hpp"

namespace ParaCompiler::Visitor {

class DumpVisitor : public Visitor {
    int indent_level = 0;

    void print_indent() const {
        for (int i = 0; i < indent_level; ++i) std::cerr << "  |";

        if (indent_level > 0) std::cerr << "-- ";
    }

    std::string type_to_str(const Types::Type *t) {
        return t ? std::string(*t) : "nullType";
    }

   public:
    void visit(AST::Program &p) override {
        print_indent();
        std::cerr << "Program\n";

        indent_level++;
        for (const auto &stmt : p.statements)
            if (stmt) stmt->accept(*this);

        indent_level--;
    }

    // --- Statements ---
    void visit(AST::Assignment &node) override {
        print_indent();
        std::cerr << "Assignment type: [" << type_to_str(node.sym->type)
                  << "] [Name: " << node.name << "]\n";

        indent_level++;
        if (node.typeSpec) node.typeSpec->accept(*this);
        if (node.val) node.val->accept(*this);
        indent_level--;
    }

    void visit(AST::Print &node) override {
        print_indent();
        std::cerr << "Output\n";

        indent_level++;
        if (node.expr) node.expr->accept(*this);
        indent_level--;
    }

    void visit(AST::Conversion &node) override {
        print_indent();
        std::cerr << "Conversion to [" << type_to_str(node.type) << "]\n";

        indent_level++;
        if (node.expr) node.expr->accept(*this);
        indent_level--;
    }

    void visit(AST::ExprStmt &node) override {
        print_indent();
        std::cerr << "ExprStmt\n";

        indent_level++;
        if (node.expr) node.expr->accept(*this);
        indent_level--;
    }

    // --- Expressions & Leaves ---
    void visit(AST::BinExpr &node) override {
        print_indent();
        std::cerr << "BinExpr type: [" << type_to_str(node.type) << "] [" << node.op
                  << "]\n";

        indent_level++;
        node.left->accept(*this);
        node.right->accept(*this);
        indent_level--;
    }

    void visit(AST::UnaryExpr &node) override {
        print_indent();
        std::cerr << "UnaryExpr type: [" << type_to_str(node.type) << "] [" << node.op
                  << "]\n";

        indent_level++;
        node.expr->accept(*this);
        indent_level--;
    }

    void visit(AST::IntLit &node) override {
        print_indent();
        std::cerr << "IntLit type: [" << type_to_str(node.type) << "] [" << node.val
                  << "]\n";
    }

    void visit(AST::Id &node) override {
        print_indent();
        std::cerr << "Id type: [" << type_to_str(node.type) << "] [" << node.val << "]\n";
    }

    void visit(AST::Input &node) override {
        print_indent();
        std::cerr << "InputExpr type: [" << type_to_str(node.type) << "]\n";
    }

    void visit(AST::TypeSpec &node) override {
        print_indent();
        std::cerr << "TypeSpec [" << node.name << "]\n";
    }

    virtual void visit(AST::Block &node) override {
        print_indent();
        std::cerr << "Block\n";

        indent_level++;
        for (const auto &stmt : node.statements)
            if (stmt) stmt->accept(*this);

        indent_level--;
    };

    virtual void visit(AST::ForStmt &node) override {
        print_indent();
        std::cerr << "For [" << node.id << "]\n";

        indent_level++;
        print_indent();
        if (node.slice.size()) {
            std::cerr << "Slice [\n";
            indent_level++;
            for (auto &slice : node.slice) slice->accept(*this);
            indent_level--;
            print_indent();
            std::cerr << "]\n";
        } else {
            std::cerr << "Container [" << node.container << "]\n";
        }
        node.body->accept(*this);
        indent_level--;
    };

    virtual void visit(AST::WhileStmt &node) override {
        print_indent();
        std::cerr << "While\n";
        indent_level++;
        node.expr->accept(*this);
        node.body->accept(*this);
        indent_level--;
    }

    virtual void visit(AST::IfStmt &node) override {
        print_indent();
        std::cerr << "If\n";
        indent_level++;
        node.expr->accept(*this);
        node.trueb->accept(*this);
        if (node.falseb) node.falseb->accept(*this);
        indent_level--;
    }

    // --- Abstract ---
    void visit(AST::Node &) override {
        print_indent();
        std::cerr << "Empty Node!\n";
    }
    void visit(AST::Statement &) override {
        print_indent();
        std::cerr << "Empty Stmnt!\n";
    }
    void visit(AST::Expr &) override {
        print_indent();
        std::cerr << "Empty Expr!\n";
    }
};

}  // namespace ParaCompiler::Visitor
