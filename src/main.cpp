#include <fstream>
#include <iostream>

#include "antlr4-runtime.h"
#include "antlr_parser.hpp"
#include "dump_visitor.hpp"  // Include the new dump pass
#include "generated/ParaCLLexer.h"
#include "generated/ParaCLParser.h"

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

    // 1. ANTLR Parsing
    antlr4::ANTLRInputStream input(stream);
    ParaCLLexer lexer(&input);
    antlr4::CommonTokenStream tokens(&lexer);
    ParaCLParser parser(&tokens);

    auto tree = parser.program();

    // Check for syntax errors
    if (parser.getNumberOfSyntaxErrors() > 0) {
        std::cerr << "Syntax errors found. Aborting.\n";
        return 1;
    }

    // 2. Build AST
    ParaCompiler::TreeBuilder builder;
    auto ast = builder.build(tree);

    if (!ast) {
        std::cerr << "Failed to build AST.\n";
        return 1;
    }

    std::cout << "=== AST Structure ===\n";

    // 3. Run Dump Pass
    ParaCompiler::Visitor::DumpVisitor dumper;
    ast->accept(dumper);

    std::cout << "=====================\n";

    return 0;
}
