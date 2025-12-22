module;

export module ParaCompiler:TypeChecker;

import std;

import :AST;
import :Symbol;
import :Visitor;
import :Types;
import :DefaultVisitor;

namespace ParaCompiler::Types {

struct TypeChecker : Visitor::DefaultVisitor {
    TypeManager &manager;

    TypeChecker(TypeManager &manager_) : manager(manager_) {}

    const Types::Type *get_from_typespec(AST::TypeSpec *spec) {
        if (spec->is_int)
            return manager.get_intt(spec->int_width);
        else if (spec->is_func) {
            std::vector<std::pair<Symbols::Symbol *, const Type *>> argts;
            for (auto &i : spec->args) {
                auto argt = get_from_typespec(i.second.get());
                i.first->type = i.first->sym->type = argt;
                argts.emplace_back(i.first->sym, argt);
            }
            return manager.get_func_type(argts, get_from_typespec(spec->ret_spec.get()));
        } else
            throw std::runtime_error("not implemented type-spec like this one: " +
                                     spec->name);
    }

    std::unique_ptr<AST::Expr> make_conversion_node_or_propagate(
        std::unique_ptr<AST::Expr> expr, const Type *t) {
        if (expr->is_lvalue()) {
            auto new_expr = std::make_unique<AST::LValToRVal>();
            new_expr->type = expr->type;
            new_expr->expr = std::move(expr);
            expr = std::move(new_expr);
        }
        if (expr->type == t) return expr;
        // if we request bool, we first request default int and then convert it to bool
        if (t == manager.get_boolt())
            expr = make_conversion_node_or_propagate(std::move(expr), manager.get_intt());

        if (expr->type == manager.get_flexiblet()) {
            expr->type = t;
            expr->accept(*this);
            return expr;
        }
        return std::make_unique<AST::Conversion>(std::move(expr), t);
    }

    void visit(AST::Print &node) override {
        node.expr->accept(*this);
        auto res_type = (node.expr->type == manager.get_flexiblet() ||
                         node.expr->type == manager.get_boolt())
                            ? manager.get_intt()
                            : node.expr->type;  // for lval->rval conversion
        node.expr = make_conversion_node_or_propagate(std::move(node.expr), res_type);
    }

    void visit(AST::Assignment &node) override {
        auto id = dynamic_cast<AST::Id *>(node.left.get());
        if (!id) {
            node.left->accept(*this);
            node.val->accept(*this);
            node.val =
                make_conversion_node_or_propagate(std::move(node.val), node.left->type);
            return;
        }
        if (node.typeSpec && id->sym->type)
            throw std::runtime_error(
                "an attempt to declare an already declared variable " + node.name +
                " which was declared with type " + std::string(*id->sym->type));
        if (!node.typeSpec && !node.val)
            throw std::runtime_error(
                "unexpected: no val and typespec for assignment node");

        if (node.typeSpec)
            node.left->type = id->type = id->sym->type =
                get_from_typespec(node.typeSpec.get());

        if (node.val) node.val->accept(*this);
        if (auto ft = dynamic_cast<const FuncType *>(id->type)) node.val->type = ft;

        if (!id->sym->type && node.val) {
            if (node.val->type != manager.get_flexiblet()) {
                id->type = id->sym->type = node.val->type;
                // lval->rval
                node.val =
                    make_conversion_node_or_propagate(std::move(node.val), id->sym->type);
            } else {
                id->type = id->sym->type = manager.get_intt();
                node.val =
                    make_conversion_node_or_propagate(std::move(node.val), id->sym->type);
            }
            return;
        }

        if (id->sym->type && !node.val) {
            id->type = id->sym->type;
            return;
        }
        id->accept(*this);

        // both val and sym.type are present
        if (auto ft = dynamic_cast<const FuncType *>(node.left->type)) {
            if (dynamic_cast<const FuncType *>(node.val->type))
                node.val = make_conversion_node_or_propagate(std::move(node.val), ft);
            else
                throw std::runtime_error("an attempt to assign non-func to func! " +
                                         Types::Type::ptr_to_str(node.val->type) +
                                         " is assigned to " +
                                         Types::Type::ptr_to_str(node.left->type));
        }

        auto comt = manager.get_common_type(id->sym->type, node.val->type);
        if (comt != id->sym->type)
            throw std::runtime_error(
                "Error: conversion does not exist from expr to assignable variable!"
                "types are: " +
                std::string(*id->sym->type) +
                " for var and: " + std::string(*node.val->type) + " for expr");
        node.val = make_conversion_node_or_propagate(std::move(node.val), comt);
    }

    virtual void visit(AST::Call &node) override {
        node.func->accept(*this);
        // lval->rval conv
        auto leftt = node.func->type;
        node.func = make_conversion_node_or_propagate(std::move(node.func), leftt);
        auto ft = dynamic_cast<const FuncType *>(node.func->type);
        if (!ft)
            throw std::runtime_error("can only call functions but got " +
                                     Types::Type::ptr_to_str(node.func->type));
        node.type = ft->res_type;
        for (int i = 0; i < node.args.size(); i++) {
            auto &arg = node.args[i];
            if (arg) {
                arg->accept(*this);
                arg =
                    make_conversion_node_or_propagate(std::move(arg), ft->args[i].second);
            }
        }
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
        // if type was set, it's propagating from above, otherwise get it from child
        if (!node.type) {
            node.expr->accept(*this);
            node.type = node.expr->type;
        }
        // do lval->rval conversion if needed
        node.expr = make_conversion_node_or_propagate(std::move(node.expr), node.type);
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
        if (is_boolop && comtype == manager.get_flexiblet()) comtype = manager.get_intt();

        if (comtype != manager.get_flexiblet()) {
            // if all is flexible, no need to propagate or change anything
            node.left = make_conversion_node_or_propagate(std::move(node.left), comtype);
            node.right =
                make_conversion_node_or_propagate(std::move(node.right), comtype);
        }
        node.type = is_arithop ? comtype : manager.get_boolt();
    }

    void visit(AST::Glue &node) override {
        std::vector<const Type *> fields;
        std::unordered_map<std::string, size_t> names;

        for (int i = 0; i < node.vals.size(); i++) {
            auto &val = node.vals[i];
            val.val->accept(*this);
            if (val.val->type == manager.get_flexiblet())
                val.val = make_conversion_node_or_propagate(std::move(val.val),
                                                            manager.get_intt());
            fields.push_back(val.val->type);
            if (!val.name.empty()) names.emplace(val.name, i);
            // do lval->rval if needed
            val.val =
                make_conversion_node_or_propagate(std::move(val.val), fields.back());
        }

        node.type = manager.get_struct_type(std::move(fields), std::move(names));
    }

    void visit(AST::DotExpr &node) override {
        node.left->accept(*this);
        auto ltype = dynamic_cast<const StructType *>(node.left->type);
        if (!ltype)
            throw std::runtime_error(
                "Can access only structs with dots but saw an attempt to access " +
                std::string(*node.left->type) + " with field " + node.id);

        auto it = ltype->names.find(node.id);
        if (it == ltype->names.end())
            throw std::runtime_error("Unknown field " + node.id + " of type " +
                                     std::string(*ltype));
        node.field_ind = it->second;
        node.type = ltype->fields[it->second];
    }

    void visit(AST::RetStmt &node) override {
        node.expr->accept(*this);
        const Type *rest = nullptr;
        if (node.expr->type == manager.get_flexiblet())
            rest = manager.get_intt();
        else
            rest = node.expr->type;
        node.expr = make_conversion_node_or_propagate(std::move(node.expr), rest);
    }

    void visit(AST::IndexExpr &node) override {
        node.left->accept(*this);
        auto ltype = dynamic_cast<const StructType *>(node.left->type);
        if (!ltype)
            throw std::runtime_error(
                "Can access only structs with [] but saw an attempt to access " +
                std::string(*node.left->type) + " with index " +
                std::to_string(node.ind));

        node.type = ltype->fields[node.ind];
    }
};

}  // namespace ParaCompiler::Types
