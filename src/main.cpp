#include <fstream>
#include <iostream>

#include "compiler.hpp"
#include "dump_visitor.hpp"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: ./paracl <file>\n";
        return 1;
    }

    std::ifstream stream(argv[1]);
    if (!stream) {
        std::cerr << "File not found: " << argv[1] << "\n";
        return 1;
    }

    bool dump_ast = false;
    for (int i = 2; i < argc; ++i) {
        if (std::string(argv[i]) == "--dump-ast") {
            dump_ast = true;
        }
    }

    ParaCompiler::Compiler compiler(stream);
    auto& ast = compiler.ast;
    if (!ast) {
        std::cerr << "Failed to build AST.\n";
        return 1;
    }
    if (dump_ast) {
        std::cerr << "=== AST Structure ===\n";
        ParaCompiler::Visitor::DumpVisitor dumper;
        ast->accept(dumper);
        std::cerr << "=====================\n";
    }

    return 0;
}
