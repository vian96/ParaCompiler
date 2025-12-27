module;

export module ParaCompiler:Symbol;

import std;

import :Types;
import :DefaultVisitor;
import :AST;
import :Visitor;

namespace ParaCompiler::Symbols {

struct Symbol {
    // i hope nothing breaks but std::string too heavy?
    using NameType = std::string_view;

    using ArenaType = std::deque<Symbols::Symbol>;
    NameType name;
    const Types::Type *type = nullptr;
    AST::Statement *def = nullptr;

    Symbol(std::string_view name_) : name(name_) {}
};

using Scope = std::unordered_map<Symbol::NameType, Symbol *>;

struct NameResolution : public Visitor::DefaultVisitor {
    std::vector<Scope> scopes;
    Symbol::ArenaType &symbols;
    // allows adding new symbols
    bool inside_declaration_left = false;

    Symbol *add_or_get_symbol(std::string_view name) {
        for (auto it = scopes.rbegin(); it != scopes.rend(); ++it)
            if (auto sym = it->find(name); sym != it->end()) return sym->second;

        if (!inside_declaration_left)
            throw std::runtime_error("an attempt to use undeclared var" +
                                     std::string(name));
        symbols.emplace_back(name);
        scopes.back().emplace(name, &symbols.back());
        return &symbols.back();
    }

    NameResolution(Symbol::ArenaType &symbols_) : symbols(symbols_) {}

    void visit(AST::Block &node) override {
        scopes.emplace_back();
        for (const auto &stmt : node.statements)
            if (stmt) stmt->accept(*this);
        scopes.pop_back();
    }

    void visit(AST::Program &p) override {
        scopes.emplace_back();
        for (const auto &stmt : p.statements)
            if (stmt) stmt->accept(*this);
    }

    void visit(AST::Assignment &node) override {
        inside_declaration_left = true;
        node.left->accept(*this);

        std::vector<Scope> prev_scopes;
        if (node.typeSpec && node.typeSpec->is_func) {
            // we clear prev scopes as function should not access external vars
            std::swap(prev_scopes, scopes);
            scopes.emplace_back();
            for (auto &i : node.typeSpec->args) i.first->accept(*this);
        }
        inside_declaration_left = false;  // allow to set func args syms

        if (node.val) node.val->accept(*this);
        if (node.typeSpec && node.typeSpec->is_func) {
            // restore old scopes stack
            std::swap(prev_scopes, scopes);
        }
    }

    void visit(AST::ForStmt &node) override {
        scopes.emplace_back();
        inside_declaration_left = true;
        node.i_sym = add_or_get_symbol(node.id);
        inside_declaration_left = false;
        if (node.container) node.container->accept(*this);
        for (auto &slice : node.slice) slice->accept(*this);
        node.body->accept(*this);
        scopes.pop_back();
    }

    void visit(AST::Id &node) override {
        auto sym = add_or_get_symbol(node.val);
        if (!sym) throw std::runtime_error("Unknown symbol used: " + node.val);
        node.sym = sym;
    }
};

}  // namespace ParaCompiler::Symbols
