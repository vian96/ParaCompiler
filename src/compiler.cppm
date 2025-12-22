module;

#include <iostream>
#include <istream>
#include <memory>
#include <cassert>

export module ParaCompiler;

export import :AST;
export import :Visitor;
export import :Symbol;
export import :Types;
export import :AntlrParser;
export import :TypeChecker;
export import :LLVMEmitter;
export import :DefaultVisitor;
export import :DumpVisitor;

export namespace ParaCompiler {

struct Compiler {
    std::unique_ptr<AST::Program> ast = nullptr;
    Symbols::Symbol::ArenaType symbols;
    Types::TypeManager type_manager;

    Compiler() {}
    Compiler(std::istream &stream) { compile_tu(stream); }

    bool compile_tu(std::istream &stream) {
        ParaCompiler::TreeBuilder builder;
        ast = builder.build(stream);
        if (!ast) {
            std::cerr << "Failed to build AST.\n";
            return false;
        }

        Symbols::NameResolution name_res(symbols);
        name_res.visit(*ast);

        Types::TypeChecker typecheck(type_manager);
        ast->accept(typecheck);

        dump_ast();

        LLVMEmitter::LLVMEmitterVisitor ir_emit(type_manager);
        ir_emit.visit(*ast);
        ir_emit.print();
        return true;
    }

    void dump_ast() {
        assert(ast);
        std::cerr << "=== AST Structure ===\n";
        ParaCompiler::Visitor::DumpVisitor dumper;
        ast->accept(dumper);
        std::cerr << "=====================\n";
    }
};

}  // namespace ParaCompiler
