module;

export module ParaCompiler:TypeChecker;

import std;

import :AST;
import :Symbol;
import :Visitor;
import :Types;
import :DefaultVisitor;
import :DeepCopy;

namespace ParaCompiler::Types {

// Scans all return statements to find the common type
struct ReturnTypeScanner : public Visitor::DefaultVisitor {
    Types::TypeManager &manager;
    const Type *common_type = nullptr;

    ReturnTypeScanner(Types::TypeManager &m) : manager(m) {}

    using DefaultVisitor::visit;

    void add_type(const Type *t) {
        if (!t) return;
        if (t == manager.get_flexiblet()) return;
        if (!common_type) {
            common_type = t;
            return;
        }
        common_type = manager.get_common_type(common_type, t);
    }

    void visit(AST::RetStmt &node) override {
        if (node.expr) add_type(node.expr->type);
    }

    // may be bad to visit nested functions because they mess up return type of our
    // function with their returns. but we just don't visit assignments of functions
    void visit(AST::FuncBody &node) override {
        if (node.body) node.body->accept(*this);
    }

    // Stop recursion at assignments that define new functions
    void visit(AST::Assignment &node) override {
        node.left->accept(*this);
        if (node.val && !dynamic_cast<AST::FuncBody *>(node.val.get()))
            node.val->accept(*this);
    }
};

struct TypeChecker;

struct ReturnTypeEnforcer : public Visitor::DefaultVisitor {
    TypeChecker &tc;
    const Type *target_type;

    ReturnTypeEnforcer(Types::TypeChecker &tc_ref, const Type *t)
        : tc(tc_ref), target_type(t) {}

    using DefaultVisitor::visit;

    // uses incomplete typeChecker
    void visit(AST::RetStmt &node) override;

    // Stop recursion at assignments that define new functions
    void visit(AST::Assignment &node) override {
        if (node.val && !dynamic_cast<AST::FuncBody *>(node.val.get()))
            node.val->accept(*this);
    }
};

struct TypeChecker : Visitor::DefaultVisitor {
    TypeManager &manager;
    std::vector<std::unique_ptr<AST::Statement>> template_instantiations;
    Symbols::Symbol::ArenaType &symbol_arena;

    TypeChecker(TypeManager &m, Symbols::Symbol::ArenaType &s_arena)
        : manager(m), symbol_arena(s_arena) {}

    const Types::Type *get_from_typespec(AST::TypeSpec *spec) {
        if (!spec) return manager.get_flexiblet();

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

    void visit(AST::Program &p) override {
        DefaultVisitor::visit(p);

        // can only add new statements after iterating over them is finished
        if (!template_instantiations.empty()) {
            p.statements.insert(p.statements.begin(),
                                std::make_move_iterator(template_instantiations.begin()),
                                std::make_move_iterator(template_instantiations.end()));
            template_instantiations.clear();
        }
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

        if (node.typeSpec && node.typeSpec->is_func) {
            // ignore generics because they're compiled at call site
            if (manager.has_generic_args(id->sym->type)) return;

            // regular func
            if (auto func_body = dynamic_cast<AST::FuncBody *>(node.val.get())) {
                finalize_function(func_body, id->sym);
                return;
            }
        }

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

    void visit(AST::Call &node) override {
        auto *func_id = dynamic_cast<AST::Id *>(node.func.get());
        Symbols::Symbol *sym = func_id ? func_id->sym : nullptr;
        bool is_generic =
            !!sym && ((!sym->type && sym->def) ||
                      (sym->type && sym->def && manager.is_generic_type(sym->type)));

        // handle arg expressions first
        std::vector<const Type *> arg_types;
        for (auto &arg : node.args) {
            arg->accept(*this);
            if (arg->type == manager.get_flexiblet())
                arg =
                    make_conversion_node_or_propagate(std::move(arg), manager.get_intt());

            arg_types.push_back(arg->type);
        }

        const Types::FuncType *final_func_type = nullptr;

        // handle template call
        if (is_generic) {
            if (auto it = manager.instantiations.find({sym, arg_types});
                it != manager.instantiations.end())
                func_id->sym = it->second;
            else
                instantiate_generic(sym, arg_types, func_id);

            func_id->type = func_id->sym->type;
            final_func_type = dynamic_cast<const FuncType *>(func_id->sym->type);
        } else {
            node.func->accept(*this);
            node.func =
                make_conversion_node_or_propagate(std::move(node.func), node.func->type);
            final_func_type = dynamic_cast<const FuncType *>(node.func->type);
        }

        if (!final_func_type)
            throw std::runtime_error("can only call functions but got " +
                                     Types::Type::ptr_to_str(node.func->type));

        node.type = final_func_type->res_type;
        if (node.args.size() != final_func_type->args.size())
            throw std::runtime_error("Argument count mismatch");

        // both lval->rval and regular conversions
        for (size_t i = 0; i < node.args.size(); ++i)
            node.args[i] = make_conversion_node_or_propagate(
                std::move(node.args[i]), final_func_type->args[i].second);
    }

    // calculates correct return type if needed and makes conversion nodes.
    // requires that at least some func type was set to func_sym
    void finalize_function(AST::FuncBody *body_node, Symbols::Symbol *func_sym) {
        if (!body_node || !func_sym) return;

        body_node->accept(*this);

        const Types::FuncType *ft = dynamic_cast<const Types::FuncType *>(func_sym->type);
        const Types::Type *target_type = ft ? ft->res_type : manager.get_flexiblet();

        // deduce type
        if (!target_type || target_type == manager.get_flexiblet()) {
            ReturnTypeScanner scanner(manager);
            body_node->accept(scanner);

            target_type = scanner.common_type;

            if (!target_type || target_type == manager.get_flexiblet())
                target_type = manager.get_intt();

            // set correct (calculated) type to symbol
            if (ft) {
                auto concrete_ft = manager.get_func_type(ft->args, target_type);
                func_sym->type = concrete_ft;
                body_node->type = concrete_ft;
            }
        } else {
            body_node->type = ft;
        }

        ReturnTypeEnforcer enforcer(*this, target_type);
        body_node->accept(enforcer);
    }

    void instantiate_generic(Symbols::Symbol *template_sym,
                             const std::vector<const Type *> &arg_types,
                             AST::Id *call_site_id) {
        auto *def_stmt = dynamic_cast<AST::Assignment *>(template_sym->def);
        if (!def_stmt) throw std::runtime_error("Generic def not found");

        ::ParaCompiler::Visitor::DeepCopyVisitor copier;
        std::unique_ptr<AST::Assignment> new_def = copier.copy(def_stmt);

        std::string new_name = std::string(template_sym->name) + "." +
                               std::to_string(std::hash<void *>{}(new_def.get()));
        symbol_arena.emplace_back(new_name);
        Symbols::Symbol *concrete_sym = &symbol_arena.back();

        auto *left_id = static_cast<AST::Id *>(new_def->left.get());
        if (!left_id)
            throw std::runtime_error(
                "an attempt to assign templated function to some expression");
        // setting up correct fields
        new_def->name = new_name;
        left_id->val = new_name;
        left_id->sym = concrete_sym;
        concrete_sym->def = new_def.get();

        Symbols::NameResolution resolver(symbol_arena);
        resolver.scopes.emplace_back();
        resolver.scopes.back().emplace(new_name, concrete_sym);
        new_def->accept(resolver);

        if (!new_def->typeSpec || new_def->typeSpec->args.size() != arg_types.size())
            throw std::runtime_error("Generic instantiation arg mismatch");

        std::vector<std::pair<Symbols::Symbol *, const Type *>> concrete_args_vec;
        for (size_t i = 0; i < arg_types.size(); ++i) {
            auto &arg_pair = new_def->typeSpec->args[i];
            Symbols::Symbol *arg_sym = arg_pair.first->sym;

            arg_sym->type = arg_types[i];
            concrete_args_vec.emplace_back(arg_sym, arg_types[i]);
        }

        auto *func_body = dynamic_cast<AST::FuncBody *>(new_def->val.get());
        if (!func_body) throw std::runtime_error("Generic func has no body");

        // don't know return type, but it'll be deduced later
        auto *proto_type =
            manager.get_func_type(concrete_args_vec, manager.get_flexiblet());
        concrete_sym->type = proto_type;
        new_def->val->type = proto_type;

        finalize_function(func_body, concrete_sym);

        call_site_id->sym = concrete_sym;
        call_site_id->type = concrete_sym->type;

        manager.instantiations[{template_sym, arg_types}] = concrete_sym;
        template_instantiations.push_back(std::move(new_def));
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

void ReturnTypeEnforcer::visit(AST::RetStmt &node) {
    if (!node.expr) return;
    node.expr = tc.make_conversion_node_or_propagate(std::move(node.expr), target_type);
}

}  // namespace ParaCompiler::Types
