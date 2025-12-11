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

    std::pair<Symbol*, bool> find_symbol(std::string_view name) {
        auto it = scopes.rbegin();
        if (auto sym = it->find(name); sym != it->end()) return {sym->second, true};

        for (++it; it != scopes.rend(); ++it)
            if (auto sym = it->find(name); sym != it->end()) return {sym->second, false};

        return {nullptr, false};
    }

    Symbol* add_or_get_symbol(std::string_view name) {
        if (auto it = scopes.back().find(name); it != scopes.back().end())
            return it->second;

        symbols.push_back(name);
        scopes.back().emplace(name, &symbols.back());
        return &symbols.back();
    }

    NameResolution(Symbol::ArenaType& symbols_) : symbols(symbols_) {}

    void visit(AST::Program& p) override {
        scopes.emplace_back();
        for (const auto& stmt : p.statements)
            if (stmt) stmt->accept(*this);
    }

    void visit(AST::Assignment& node) override {
        node.sym = add_or_get_symbol(node.name);
        if (node.val) node.val->accept(*this);
    }

    void visit(AST::Id& node) override {
        auto sym = find_symbol(node.val).first;
        if (!sym) throw std::runtime_error("Unknown symbol used: " + node.val);
        node.sym = sym;
    }
};

}  // namespace ParaCompiler::Symbols
