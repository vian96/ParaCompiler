#pragma once

#include <deque>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>

#include "ast.hpp"
#include "default_visitor.hpp"
#include "visitor.hpp"

namespace ParaCompiler::Symbols {

struct Symbol {
    // i hope nothing breaks but std::string too heavy?
    using NameType = std::string_view;

    using ArenaType = std::deque<Symbols::Symbol>;
    NameType name;
    void* type;
    AST::Statement* def;

    Symbol(std::string_view name_) : name(name_) {}
};

using Scope = std::unordered_map<Symbol::NameType, Symbol*>;

struct NameResolution : public Visitor::DefaultVisitor {
    std::vector<Scope> scopes;
    Symbol::ArenaType& symbols;

    Symbol* add_or_get_symbol(std::string_view name) {
        for (auto it = scopes.rbegin(); it != scopes.rend(); ++it)
            if (auto sym = it->find(name); sym != it->end()) return sym->second;

        symbols.emplace_back(name);
        scopes.back().emplace(name, &symbols.back());
        return &symbols.back();
    }

    NameResolution(Symbol::ArenaType& symbols_) : symbols(symbols_) {}

    void visit(AST::Block& node) override {
        scopes.emplace_back();
        for (const auto& stmt : node.statements)
            if (stmt) stmt->accept(*this);
        scopes.pop_back();
    }

    void visit(AST::Program& p) override {
        scopes.emplace_back();
        for (const auto& stmt : p.statements)
            if (stmt) stmt->accept(*this);
    }

    void visit(AST::Assignment& node) override {
        node.sym = add_or_get_symbol(node.name);
        if (node.val) node.val->accept(*this);
    }

    void visit(AST::ForStmt& node) override {
        scopes.emplace_back();
        node.i_sym = add_or_get_symbol(node.id);
        if (node.container) node.container->accept(*this);
        for (auto& slice : node.slice) slice->accept(*this);
        node.body->accept(*this);
        scopes.pop_back();
    }

    void visit(AST::Id& node) override {
        auto sym = add_or_get_symbol(node.val);
        if (!sym) throw std::runtime_error("Unknown symbol used: " + node.val);
        node.sym = sym;
    }
};

}  // namespace ParaCompiler::Symbols
