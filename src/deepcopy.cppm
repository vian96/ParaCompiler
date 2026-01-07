module;

export module ParaCompiler:DeepCopy;

import std;
import :AST;
import :Visitor;

export namespace ParaCompiler::Visitor {

// Does deepcopy of AST, ignoring symbols and types
class DeepCopyVisitor : public Visitor {
    std::unique_ptr<AST::Node> result;

   public:
    template <typename T>
    std::unique_ptr<T> copy(const T *node) {
        if (!node) return nullptr;

        // const_cast is safe here: no modification, only read is done in visitor
        const_cast<T *>(node)->accept(*this);

        if (!result) throw std::runtime_error("DeepCopy failed to produce a result");

        AST::Node *raw = result.release();
        T *typed = dynamic_cast<T *>(raw);

        if (!typed && raw) {
            // should not happen but just in case
            delete raw;
            throw std::runtime_error("DeepCopy type mismatch error");
        }

        return std::unique_ptr<T>(typed);
    }

    template <typename T>
    void copy_vec(const std::vector<std::unique_ptr<T>> &src,
                  std::vector<std::unique_ptr<T>> &dest) {
        dest.reserve(src.size());
        for (const auto &item : src) dest.push_back(copy(item.get()));
    }

    void visit(AST::Program &node) override {
        auto n = std::make_unique<AST::Program>();
        copy_vec(node.statements, n->statements);
        result = std::move(n);
    }

    void visit(AST::Block &node) override {
        auto n = std::make_unique<AST::Block>();
        copy_vec(node.statements, n->statements);
        result = std::move(n);
    }

    void visit(AST::TypeSpec &node) override {
        auto n = std::make_unique<AST::TypeSpec>();
        n->name = node.name;
        n->is_int = node.is_int;
        n->int_width = node.int_width;
        n->is_func = node.is_func;

        for (const auto &pair : node.args)
            n->args.emplace_back(copy(pair.first.get()), copy(pair.second.get()));

        n->ret_spec = copy(node.ret_spec.get());
        result = std::move(n);
    }

    void visit(AST::Assignment &node) override {
        auto n = std::make_unique<AST::Assignment>();
        n->name = node.name;
        n->typeSpec = copy(node.typeSpec.get());
        n->left = copy(node.left.get());
        n->val = copy(node.val.get());
        result = std::move(n);
    }

    void visit(AST::Print &node) override {
        auto n = std::make_unique<AST::Print>();
        n->expr = copy(node.expr.get());
        result = std::move(n);
    }

    void visit(AST::ExprStmt &node) override {
        auto n = std::make_unique<AST::ExprStmt>();
        n->expr = copy(node.expr.get());
        result = std::move(n);
    }

    void visit(AST::RetStmt &node) override {
        auto n = std::make_unique<AST::RetStmt>();
        n->expr = copy(node.expr.get());
        result = std::move(n);
    }

    void visit(AST::IfStmt &node) override {
        auto n = std::make_unique<AST::IfStmt>();
        n->expr = copy(node.expr.get());
        n->trueb = copy(node.trueb.get());
        n->falseb = copy(node.falseb.get());
        result = std::move(n);
    }

    void visit(AST::WhileStmt &node) override {
        auto n = std::make_unique<AST::WhileStmt>();
        n->expr = copy(node.expr.get());
        n->body = copy(node.body.get());
        result = std::move(n);
    }

    void visit(AST::ForStmt &node) override {
        auto n = std::make_unique<AST::ForStmt>();
        n->id = node.id;
        n->container = copy(node.container.get());
        copy_vec(node.slice, n->slice);
        n->body = copy(node.body.get());
        n->i_sym = nullptr;
        result = std::move(n);
    }

    void visit(AST::BinExpr &node) override {
        auto n = std::make_unique<AST::BinExpr>();
        n->op = node.op;
        n->left = copy(node.left.get());
        n->right = copy(node.right.get());
        n->type = nullptr;
        result = std::move(n);
    }

    void visit(AST::UnaryExpr &node) override {
        auto n = std::make_unique<AST::UnaryExpr>();
        n->op = node.op;
        n->expr = copy(node.expr.get());
        n->type = nullptr;
        result = std::move(n);
    }

    void visit(AST::IntLit &node) override {
        auto n = std::make_unique<AST::IntLit>();
        n->val = node.val;
        n->type = nullptr;
        result = std::move(n);
    }

    void visit(AST::Id &node) override {
        auto n = std::make_unique<AST::Id>(node.val);
        n->sym = nullptr;
        n->type = nullptr;
        result = std::move(n);
    }

    void visit(AST::Input &node) override {
        auto n = std::make_unique<AST::Input>();
        n->type = nullptr;
        result = std::move(n);
    }

    void visit(AST::Glue &node) override {
        auto n = std::make_unique<AST::Glue>();
        for (const auto &entry : node.vals) {
            n->vals.push_back(AST::GlueEntry{entry.name, copy(entry.val.get())});
        }
        n->type = nullptr;
        result = std::move(n);
    }

    void visit(AST::DotExpr &node) override {
        auto n = std::make_unique<AST::DotExpr>();
        n->left = copy(node.left.get());
        n->id = node.id;
        n->field_ind = node.field_ind;
        n->type = nullptr;
        result = std::move(n);
    }

    void visit(AST::IndexExpr &node) override {
        auto n = std::make_unique<AST::IndexExpr>();
        n->left = copy(node.left.get());
        n->ind = node.ind;
        n->type = nullptr;
        result = std::move(n);
    }

    void visit(AST::Call &node) override {
        auto n = std::make_unique<AST::Call>();
        n->func = copy(node.func.get());
        copy_vec(node.args, n->args);
        n->type = nullptr;
        result = std::move(n);
    }

    void visit(AST::FuncBody &node) override {
        auto n = std::make_unique<AST::FuncBody>();
        n->body = copy(node.body.get());
        n->type = nullptr;
        result = std::move(n);
    }

    void visit(AST::Conversion &node) override {
        auto n = std::make_unique<AST::Conversion>(copy(node.expr.get()), node.type);
        result = std::move(n);
    }

    void visit(AST::LValToRVal &node) override {
        auto n = std::make_unique<AST::LValToRVal>();
        n->expr = copy(node.expr.get());
        n->type = nullptr;
        result = std::move(n);
    }

    void visit(AST::Node &) override {
        throw std::runtime_error(
            "DeepCopyVisitor: Unimplemented AST Node type encountered.");
    }
    void visit(AST::Statement &) override {
        throw std::runtime_error("DeepCopyVisitor: Unimplemented Statement type.");
    }
    void visit(AST::Expr &) override {
        throw std::runtime_error("DeepCopyVisitor: Unimplemented Expr type.");
    }
};

}  // namespace ParaCompiler::Visitor
