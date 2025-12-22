#include <istream>
#include <memory>

#include "antlr_parser.hpp"
#include "ast.hpp"
#include "llvm_emitter.hpp"
#include "symbol.hpp"
#include "type_checker.hpp"

namespace ParaCompiler {

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

        LLVMEmitter::LLVMEmitterVisitor ir_emit(type_manager);
        ir_emit.visit(*ast);
        ir_emit.print();
        return true;
    }
};

}  // namespace ParaCompiler
