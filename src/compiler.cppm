module;

export module ParaCompiler:Compiler;

import std;

import :AST;
import :Visitor;
import :Symbol;
import :Types;
import :AntlrParser;
import :TypeChecker;
import :DumpVisitor;

export namespace ParaCompiler {

struct Compiler {
    std::unique_ptr<AST::Program> ast = nullptr;
    Symbols::Symbol::ArenaType symbols;
    Types::TypeManager type_manager;

    Compiler() = default;

    bool compile_tu(std::istream &stream) {
        ParaCompiler::TreeBuilder builder;
        ast = builder.build(stream);
        if (!ast) {
            std::cerr << "Failed to build AST.\n";
            return false;
        }

        try {
            Symbols::NameResolution name_res(symbols);
            name_res.visit(*ast);

            Types::TypeChecker typecheck(type_manager);
            ast->accept(typecheck);
        } catch (const std::exception& e) {
            std::cerr << "Semantic error: " << e.what() << "\n";
            return false;
        }

        return true;
    }

    void dump_ast() {
        if (!ast) {
            throw std::runtime_error("AST is null");
        }
        std::cerr << "=== AST Structure ===\n";
        ParaCompiler::Visitor::DumpVisitor dumper;
        ast->accept(dumper);
        std::cerr << "=====================\n";
    }
};

}  // namespace ParaCompiler
