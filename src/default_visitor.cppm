module;

#include <vector>

export module ParaCompiler:DefaultVisitor;

import :AST;
import :Visitor;

export namespace ParaCompiler::Visitor {

class DefaultVisitor : public Visitor {
   public:
    void visit(AST::Program &p) override {
        for (const auto &stmt : p.statements)
            if (stmt) stmt->accept(*this);
    }

    void visit(AST::Assignment &node) override {
        node.left->accept(*this);
        if (node.typeSpec) node.val->accept(*this);
        if (node.val) node.val->accept(*this);
    }

    void visit(AST::Print &node) override {
        if (node.expr) node.expr->accept(*this);
    }

    void visit(AST::Conversion &node) override {
        if (node.expr) node.expr->accept(*this);
    }

    void visit(AST::ExprStmt &node) override {
        if (node.expr) node.expr->accept(*this);
    }

    void visit(AST::BinExpr &node) override {
        node.left->accept(*this);
        node.right->accept(*this);
    }

    void visit(AST::UnaryExpr &node) override { node.expr->accept(*this); }

    void visit(AST::Glue &node) override {
        for (auto &val : node.vals) val.val->accept(*this);
    }
    void visit(AST::DotExpr &node) override { node.left->accept(*this); }
    void visit(AST::LValToRVal &node) override { node.expr->accept(*this); }
    void visit(AST::IndexExpr &node) override { node.left->accept(*this); }

    void visit(AST::Node &) override {}
    void visit(AST::Expr &) override {}
    void visit(AST::Statement &) override {}
    void visit(AST::TypeSpec &) override {}

    void visit(AST::IntLit &) override {}
    void visit(AST::Id &) override {}
    void visit(AST::Input &) override {}

    virtual void visit(AST::Block &node) override {
        for (const auto &stmt : node.statements)
            if (stmt) stmt->accept(*this);
    }
    virtual void visit(AST::ForStmt &node) override {
        for (auto &slice : node.slice) slice->accept(*this);
        if (node.container) node.container->accept(*this);
        node.body->accept(*this);
    }
    virtual void visit(AST::WhileStmt &node) override {
        node.expr->accept(*this);
        node.body->accept(*this);
    }
    virtual void visit(AST::IfStmt &node) override {
        node.expr->accept(*this);
        node.trueb->accept(*this);
        if (node.falseb) node.falseb->accept(*this);
    }
    virtual void visit(AST::FuncBody &node) override { node.body->accept(*this); }
    virtual void visit(AST::RetStmt &node) override { node.expr->accept(*this); }
};

}  // namespace ParaCompiler::Visitor
