#include <fstream>
#include <iostream>

#include "antlr_parser.hpp"
#include "antlr4-runtime.h"
#include "default_visitor.hpp"  // Ensure you update DefaultVisitor with new methods!
#include "generated/ParaCLLexer.h"
#include "generated/ParaCLParser.h"

// Updates for DefaultVisitor (Paste into default_visitor.hpp):
/*
  void visit(AST::Assignment &) override {}
  void visit(AST::Print &) override {}
  void visit(AST::ExprStmt &node) override { if(node.expr) node.expr->accept(*this); }
*/

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: ./paracl <file>\n";
        return 1;
    }

    std::ifstream stream(argv[1]);
    if (!stream) {
        std::cerr << "File not found\n";
        return 1;
    }

    antlr4::ANTLRInputStream input(stream);
    ParaCLLexer lexer(&input);
    antlr4::CommonTokenStream tokens(&lexer);
    ParaCLParser parser(&tokens);

    auto tree = parser.program();

    ParaCompiler::TreeBuilder builder;
    auto ast = builder.build(tree);

    std::cout << "Successfully built AST with " << ast->statements.size()
              << " statements.\n";

    return 0;
}
