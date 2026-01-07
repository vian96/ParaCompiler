module;

export module ParaCompiler:DumpVisitor;

import std;

import :AST;
import :Symbol;
import :Visitor;
import :Types;
import :DefaultVisitor;

export namespace ParaCompiler::Visitor {

class DumpVisitor : public Visitor {
    int indent_level = 0;

    void print_indent() const {
        for (int i = 0; i < indent_level; ++i) std::cerr << "  |";

        if (indent_level > 0) std::cerr << "-- ";
    }

   public:
    void visit(AST::Program &p) override {
        print_indent();
        std::cerr << "Program\n";

        indent_level++;
        for (const auto &stmt : p.statements)
            if (stmt) stmt->accept(*this);

        indent_level--;
    }

    // --- Statements ---
    void visit(AST::Assignment &node) override {
        print_indent();
        std::cerr << "Assignment type: [" << Types::Type::ptr_to_str(node.left->type)
                  << "] [Name: " << node.name << "]\n";

        indent_level++;
        node.left->accept(*this);
        if (node.typeSpec) node.typeSpec->accept(*this);
        if (node.val) node.val->accept(*this);
        indent_level--;
    }

    void visit(AST::Print &node) override {
        print_indent();
        std::cerr << "Output\n";

        indent_level++;
        if (node.expr) node.expr->accept(*this);
        indent_level--;
    }

    void visit(AST::Conversion &node) override {
        print_indent();
        std::cerr << "Conversion to [" << Types::Type::ptr_to_str(node.type) << "]\n";

        indent_level++;
        if (node.expr) node.expr->accept(*this);
        indent_level--;
    }

    void visit(AST::ExprStmt &node) override {
        print_indent();
        std::cerr << "ExprStmt\n";

        indent_level++;
        if (node.expr) node.expr->accept(*this);
        indent_level--;
    }

    // --- Expressions & Leaves ---
    void visit(AST::BinExpr &node) override {
        print_indent();
        std::cerr << "BinExpr type: [" << Types::Type::ptr_to_str(node.type) << "] ["
                  << node.op << "]\n";

        indent_level++;
        node.left->accept(*this);
        node.right->accept(*this);
        indent_level--;
    }

    void visit(AST::UnaryExpr &node) override {
        print_indent();
        std::cerr << "UnaryExpr type: [" << Types::Type::ptr_to_str(node.type) << "] ["
                  << node.op << "]\n";

        indent_level++;
        node.expr->accept(*this);
        indent_level--;
    }

    void visit(AST::IntLit &node) override {
        print_indent();
        std::cerr << "IntLit type: [" << Types::Type::ptr_to_str(node.type) << "] ["
                  << node.val << "]\n";
    }

    void visit(AST::Id &node) override {
        print_indent();
        std::cerr << "Id type: [" << Types::Type::ptr_to_str(node.type) << "] ["
                  << node.val << "] sym:[" << node.sym << "]\n";
    }

    void visit(AST::Input &node) override {
        print_indent();
        std::cerr << "InputExpr type: [" << Types::Type::ptr_to_str(node.type) << "]\n";
    }

    void visit(AST::TypeSpec &node) override {
        print_indent();
        std::cerr << "TypeSpec [" << node.name << "]\n";
        indent_level++;
        for (auto &i : node.args) i.first->accept(*this);
        indent_level--;
    }

    virtual void visit(AST::Block &node) override {
        print_indent();
        std::cerr << "Block\n";

        indent_level++;
        for (const auto &stmt : node.statements)
            if (stmt) stmt->accept(*this);

        indent_level--;
    };

    virtual void visit(AST::Call &node) override {
        print_indent();
        std::cerr << "Call\n";

        indent_level++;
        if (node.func) node.func->accept(*this);
        for (const auto &arg : node.args)
            if (arg) arg->accept(*this);

        indent_level--;
    };

    virtual void visit(AST::ForStmt &node) override {
        print_indent();
        std::cerr << "For [" << node.id << "] sym:[" << node.i_sym << "]\n";

        indent_level++;
        print_indent();
        if (node.slice.size()) {
            std::cerr << "Slice [\n";
            indent_level++;
            for (auto &slice : node.slice) slice->accept(*this);
            indent_level--;
            print_indent();
            std::cerr << "]\n";
        } else {
            std::cerr << "Container [" << node.container << "]\n";
        }
        node.body->accept(*this);
        indent_level--;
    };

    virtual void visit(AST::WhileStmt &node) override {
        print_indent();
        std::cerr << "While\n";
        indent_level++;
        node.expr->accept(*this);
        node.body->accept(*this);
        indent_level--;
    }

    virtual void visit(AST::IfStmt &node) override {
        print_indent();
        std::cerr << "If\n";
        indent_level++;
        node.expr->accept(*this);
        node.trueb->accept(*this);
        if (node.falseb) node.falseb->accept(*this);
        indent_level--;
    }

    // --- Abstract ---
    void visit(AST::Node &) override {
        print_indent();
        std::cerr << "Empty Node!\n";
    }
    void visit(AST::Statement &) override {
        print_indent();
        std::cerr << "Empty Stmnt!\n";
    }
    void visit(AST::Expr &) override {
        print_indent();
        std::cerr << "Empty Expr!\n";
    }

    void visit(AST::Glue &node) override {
        print_indent();
        std::cerr << "Glue:";
        if (node.type) std::cerr << " [" << std::string(*node.type) << "]";
        std::cerr << '\n';
        indent_level++;
        for (auto &val : node.vals) val.val->accept(*this);
        indent_level--;
    }
    void visit(AST::DotExpr &node) override {
        print_indent();
        std::cerr << "DotExpr: Id: " << node.id << ", Ind: " << node.field_ind << '\n';
        indent_level++;
        node.left->accept(*this);
        indent_level--;
    }
    void visit(AST::LValToRVal &node) override {
        print_indent();
        std::cerr << "LValToRVal\n";
        indent_level++;
        node.expr->accept(*this);
        indent_level--;
    }
    void visit(AST::IndexExpr &node) override {
        print_indent();
        std::cerr << "IndexExpr: Ind: " << node.ind << '\n';
        indent_level++;
        node.left->accept(*this);
        indent_level--;
    }
    virtual void visit(AST::FuncBody &node) override {
        print_indent();
        std::cerr << "FuncBody\n";
        indent_level++;
        node.body->accept(*this);
        indent_level--;
    }
    virtual void visit(AST::RetStmt &node) override {
        print_indent();
        std::cerr << "RetStmt\n";
        indent_level++;
        node.expr->accept(*this);
        indent_level--;
    }
};

}  // namespace ParaCompiler::Visitor
