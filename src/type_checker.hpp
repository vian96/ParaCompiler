#pragma once

#include <memory>
#include <set>
#include <stdexcept>
#include <string>

#include "ast.hpp"
#include "default_visitor.hpp"
#include "symbol.hpp"
#include "types.hpp"

namespace ParaCompiler::Types {

struct TypeChecker : Visitor::DefaultVisitor {
    TypeManager &manager;

    TypeChecker(TypeManager &manager_) : manager(manager_) {}

    std::unique_ptr<AST::Expr> make_conversion_node_or_propagate(
        std::unique_ptr<AST::Expr> expr, const Type *t) {
        if (expr->type == t) return expr;
        if (expr->type == manager.get_flexiblet()) {
            expr->type = t;
            expr->accept(*this);
            return expr;
        }
        return std::make_unique<AST::Conversion>(std::move(expr), t);
    }

    void visit(AST::Print &node) override {
        node.expr->accept(*this);
        if (node.expr->type == manager.get_flexiblet())
            node.expr = make_conversion_node_or_propagate(std::move(node.expr),
                                                          manager.get_intt());
    }

    void visit(AST::Assignment &node) override {
        if (node.val) node.val->accept(*this);
        if (node.typeSpec) {
            if (node.typeSpec->is_int)
                node.sym->type = manager.get_intt(node.typeSpec->int_width);
            else
                throw std::runtime_error(
                    "not implemented not-int type-spec like this one: " +
                    node.typeSpec->name);
        }
        if (!node.sym->type && node.val) {
            if (node.val->type != manager.get_flexiblet())
                node.sym->type = node.val->type;
            else {
                node.sym->type = manager.get_intt();
                node.val = make_conversion_node_or_propagate(std::move(node.val),
                                                             node.sym->type);
            }
            return;
        }
        if (!node.sym->type && !node.val)
            throw std::runtime_error(
                "unexpected: no val and typespec for assignment node");
        if (node.sym->type && !node.val) return;
        // both val and sym.type are present
        auto comt = manager.get_common_type(node.sym->type, node.val->type);
        if (comt != node.sym->type)
            throw std::runtime_error(
                "Error: conversion does not exist from expr to assignable variable!"
                "types are: " +
                std::string(*node.sym->type) +
                " for var and: " + std::string(*node.val->type) + " for expr");
        node.val = make_conversion_node_or_propagate(std::move(node.val), comt);
    }

    void visit(AST::IfStmt &node) override {
        node.expr->accept(*this);
        node.expr =
            make_conversion_node_or_propagate(std::move(node.expr), manager.get_boolt());
        node.trueb->accept(*this);
        if (node.falseb) node.falseb->accept(*this);
    }

    void visit(AST::WhileStmt &node) override {
        node.expr->accept(*this);
        node.expr =
            make_conversion_node_or_propagate(std::move(node.expr), manager.get_boolt());
        node.body->accept(*this);
    }

    void visit(AST::ForStmt &node) override {
        if (node.container) node.container->accept(*this);
        const Type *comt = manager.get_flexiblet();  // smallest
        for (auto &slice : node.slice) {
            slice->accept(*this);
            comt = manager.get_common_type(slice->type, comt);
        }
        if (comt == manager.get_flexiblet()) comt = manager.get_intt();
        node.i_sym->type = comt;
        for (auto &slice : node.slice)
            slice = make_conversion_node_or_propagate(std::move(slice), comt);
        node.body->accept(*this);
    }

    void visit(AST::Input &node) override {
        if (!node.type) node.type = manager.get_flexiblet();
    }

    void visit(AST::IntLit &node) override {
        if (!node.type) node.type = manager.get_flexiblet();
    }

    void visit(AST::Id &node) override { node.type = node.sym->type; }

    void visit(AST::UnaryExpr &node) override {
        if (node.type)  // type was set, it's propagating from above
            node.expr->type = node.type;
        node.expr->accept(*this);
        node.type = node.expr->type;
    }

    void visit(AST::BinExpr &node) override {
        // FIXME: i think if i do bool + input, it'll cast to bool both?
        if (node.type) {  // type was set, it's propagating from above
            if (node.left->type == manager.get_flexiblet()) {
                node.left->type = node.type;
                node.left->accept(*this);
            }
            if (node.right->type == manager.get_flexiblet()) {
                node.right->type = node.type;
                node.right->accept(*this);
            }
            return;
        }
        node.left->accept(*this);
        node.right->accept(*this);

        // is there way to make it constexpr static?
        std::set<std::string> arithop = {"+", "-", "*", "/", "&&", "||"};
        std::set<std::string> boolop = {"<", ">", "==", "<=", ">=", "!="};

        // TODO: not always common type
        auto comtype = manager.get_common_type(node.left->type, node.right->type);

        bool is_boolop = boolop.contains(node.op);
        bool is_arithop = arithop.contains(node.op);
        if (!is_arithop && !is_boolop) throw std::runtime_error("unknown op: " + node.op);

        // bool op on two inputs or intlit has to be regular int
        if (is_boolop && comtype == manager.get_flexiblet())
            comtype = manager.get_intt();

        if (comtype != manager.get_flexiblet()) {
            // if all is flexible, no need to propagate or change anything
            node.left = make_conversion_node_or_propagate(std::move(node.left), comtype);
            node.right =
                make_conversion_node_or_propagate(std::move(node.right), comtype);
        }
        node.type = is_arithop ? comtype : manager.get_boolt();
    }
};

}  // namespace ParaCompiler::Types
