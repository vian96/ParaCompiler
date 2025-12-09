#include <ANTLRInputStream.h>
#include <llvm/Support/raw_ostream.h>

#include <deque>
#include <istream>
#include <memory>

#include "ParaCLLexer.h"
#include "ParaCLParser.h"
#include "antlr_parser.hpp"
#include "ast.hpp"
#include "llvm_emitter.hpp"
#include "symbol.hpp"
#include "debug_print.hpp"

namespace ParaCompiler {

struct Compiler {
    std::unique_ptr<AST::Program> ast = nullptr;
    Symbols::Symbol::ArenaType symbols;

    Compiler() {}
    Compiler(std::istream &stream) { compile_tu(stream); }

    bool compile_tu(std::istream &stream) {
        antlr4::ANTLRInputStream input(stream);
        ParaCLLexer lexer(&input);
        antlr4::CommonTokenStream tokens(&lexer);
        ParaCLParser parser(&tokens);

        auto tree = parser.program();
        if (parser.getNumberOfSyntaxErrors() > 0) {
            std::cerr << "Syntax errors found. Aborting.\n";
            return false;
        }

        ParaCompiler::TreeBuilder builder;
        ast = builder.build(tree);
        if (!ast) {
            std::cerr << "Failed to build AST.\n";
            return false;
        }

        Symbols::NameResolution name_res(symbols);
        name_res.visit(*ast);

        LLVMEmitter::LLVMEmitterVisitor ir_emit;
        ir_emit.visit(*ast);
        ir_emit.module.print(llvm::outs(), nullptr);
        return true;
    }
};

}  // namespace ParaCompiler
