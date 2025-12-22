module;

#include <any>
#include <iostream>
#include <istream>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

#include "ParaCLBaseVisitor.h"
#include "ParaCLLexer.h"
#include "ParaCLParser.h"

export module ParaCompiler:AntlrParser;

import :AST;
import :Types;

export namespace ParaCompiler {

using Any = std::any;

class TreeBuilder : public ParaCLBaseVisitor {
   public:
    bool is_in_func = false;  // for transforming inputs into ids
    AST::TypeSpec* func_spec = nullptr;

    // Helper: Extract Raw Pointer from std::any, cast to T*, wrap in unique_ptr
    template <typename T>
    std::unique_ptr<T> take(Any a) {
        if (!a.has_value()) return nullptr;
        AST::Node* base_ptr = std::any_cast<AST::Node*>(a);
        T* typed_ptr = dynamic_cast<T*>(base_ptr);
        if (!typed_ptr && base_ptr != nullptr) {
            std::cerr << "ICE: AST type mismatch.\n";
            return nullptr;
        }
        return std::unique_ptr<T>(typed_ptr);
    }

    std::unique_ptr<AST::Program> build(std::istream& stream) {
        antlr4::ANTLRInputStream input(stream);
        ParaCLLexer lexer(&input);
        antlr4::CommonTokenStream tokens(&lexer);
        ParaCLParser parser(&tokens);

        auto tree = parser.program();
        if (parser.getNumberOfSyntaxErrors() > 0) {
            std::cerr << "Syntax errors found. Aborting.\n";
            return nullptr;
        }

        ParaCompiler::TreeBuilder builder;
        return builder.build(tree);
    }

    std::unique_ptr<AST::Program> build(ParaCLParser::ProgramContext* ctx) {
        return take<AST::Program>(visitProgram(ctx));
    }

   protected:
    Any visitProgram(ParaCLParser::ProgramContext* ctx) override {
        auto* node = new AST::Program();
        for (auto* stmtCtx : ctx->statement())
            if (auto stmt = take<AST::Statement>(visit(stmtCtx)))
                node->statements.push_back(std::move(stmt));

        return static_cast<AST::Node*>(node);
    }

    Any visitStatement(ParaCLParser::StatementContext* ctx) override {
        if (ctx->forStatement()) return visit(ctx->forStatement());

        if (ctx->ifStatement()) return visit(ctx->ifStatement());

        if (ctx->whileStatement()) return visit(ctx->whileStatement());

        if (ctx->assignment()) return visit(ctx->assignment());

        if (ctx->returnStatement()) {
            auto* node = new AST::RetStmt();
            node->expr = take<AST::Expr>(visit(ctx->expr()));
            return static_cast<AST::Node*>(node);
        }

        if (ctx->output()) return visit(ctx->output());

        if (ctx->expr()) {
            auto* node = new AST::ExprStmt();
            node->expr = take<AST::Expr>(visit(ctx->expr()));
            return static_cast<AST::Node*>(node);
        }
        return {};  // Return empty std::any instead of nullptr
    }

    Any visitAssignment(ParaCLParser::AssignmentContext* ctx) override {
        auto* node = new AST::Assignment();
        node->left = take<AST::Expr>(visit(ctx->expr(0)));
        if (auto id = dynamic_cast<AST::Id*>(node->left.get()))
            node->name = id->val;
        else if (ctx->typeSpec())
            throw std::runtime_error("can only specify type on vardecl");
        if (!node->left->is_lvalue()) throw std::runtime_error("can't assign to rvalue");

        if (ctx->typeSpec()) node->typeSpec = take<AST::TypeSpec>(visit(ctx->typeSpec()));

        if (ctx->expr().size() > 1) {
            if (node->typeSpec && node->typeSpec->is_func) {
                is_in_func = true;
                func_spec = node->typeSpec.get();
            }
            node->val = take<AST::Expr>(visit(ctx->expr(1)));
            if (is_in_func) {
                auto func = std::make_unique<AST::FuncBody>();
                func->body = std::make_unique<AST::Block>();
                auto ret = std::make_unique<AST::RetStmt>();
                ret->expr = std::move(node->val);
                func->body->statements.push_back(std::move(ret));
            }
            is_in_func = false;
            func_spec = nullptr;
        }

        return static_cast<AST::Node*>(node);
    }

    Any visitBlock(ParaCLParser::BlockContext* ctx) override {
        auto* node = new AST::Block();
        for (auto* stmtCtx : ctx->statement())
            if (auto stmt = take<AST::Statement>(visit(stmtCtx)))
                node->statements.push_back(std::move(stmt));
        return static_cast<AST::Node*>(node);
    }

    Any visitIfStatement(ParaCLParser::IfStatementContext* ctx) override {
        auto* node = new AST::IfStmt();
        node->expr = take<AST::Expr>(visit(ctx->expr()));
        node->trueb = take<AST::Block>(visit(ctx->block(0)));
        if (ctx->block().size() > 1)
            node->falseb = take<AST::Block>(visit(ctx->block(1)));
        return static_cast<AST::Node*>(node);
    }

    Any visitWhileStatement(ParaCLParser::WhileStatementContext* ctx) override {
        auto* node = new AST::WhileStmt();
        node->expr = take<AST::Expr>(visit(ctx->expr()));
        node->body = take<AST::Block>(visit(ctx->block()));
        return static_cast<AST::Node*>(node);
    }

    Any visitForStatement(ParaCLParser::ForStatementContext* ctx) override {
        auto* node = new AST::ForStmt();
        for (auto* expr : ctx->expr())
            node->slice.push_back(take<AST::Expr>(visit(expr)));
        node->id = ctx->ID()->getText();
        node->body = take<AST::Block>(visit(ctx->block()));
        return static_cast<AST::Node*>(node);
    }

    Any visitOutput(ParaCLParser::OutputContext* ctx) override {
        if (ctx->INT()->getText() != "0")
            throw std::runtime_error("output only takes 0 as the first arg");
        auto* node = new AST::Print();
        node->expr = take<AST::Expr>(visit(ctx->expr()));
        return static_cast<AST::Node*>(node);
    }

    Any visitTypeSpec(ParaCLParser::TypeSpecContext* ctx) override {
        auto* node = new AST::TypeSpec();
        node->name = ctx->getText();
        if (ctx->children[1]->getText() == "int") {
            node->is_int = true;
            node->is_func = false;
            node->int_width = ctx->INT() ? std::stoll(ctx->INT()->getText()) : 32;
        } else {
            node->is_int = false;
            node->is_func = true;
            for (int i = 0; i < ctx->ID().size(); i++)
                node->args.emplace_back(std::string(ctx->ID(i)->getText()),
                                        take<AST::TypeSpec>(ctx->typeSpec(i)));
        }
        return static_cast<AST::Node*>(node);
    }

    Any visitIntExpr(ParaCLParser::IntExprContext* ctx) override {
        auto* node = new AST::IntLit();
        try {
            node->val = std::stoll(ctx->INT()->getText());  // TODO: very long ints
        } catch (...) {
            std::cerr << "WARNING: number " << ctx->INT()->getText()
                      << " was not parsed\n";
            node->val = 0;
        }
        return static_cast<AST::Node*>(node);
    }

    Any visitUnaryExpr(ParaCLParser::UnaryExprContext* ctx) override {
        auto* node = new AST::UnaryExpr();
        node->op = ctx->children[0]->getText().at(0);
        return static_cast<AST::Node*>(node);
    }

    template <typename T>
    Any visitBinaryT(T* ctx) {
        auto* node = new AST::BinExpr();
        node->op = ctx->children[1]->getText();
        node->left = take<AST::Expr>(visit(ctx->expr()[0]));
        node->right = take<AST::Expr>(visit(ctx->expr()[1]));
        return static_cast<AST::Node*>(node);
    }

    Any visitOrExpr(ParaCLParser::OrExprContext* ctx) override {
        return visitBinaryT(ctx);
    }

    Any visitMulExpr(ParaCLParser::MulExprContext* ctx) override {
        return visitBinaryT(ctx);
    }

    Any visitAddExpr(ParaCLParser::AddExprContext* ctx) override {
        return visitBinaryT(ctx);
    }

    Any visitCmpExpr(ParaCLParser::CmpExprContext* ctx) override {
        return visitBinaryT(ctx);
    }

    Any visitAndExpr(ParaCLParser::AndExprContext* ctx) override {
        return visitBinaryT(ctx);
    }

    Any visitBracketExpr(ParaCLParser::BracketExprContext* ctx) override {
        return visit(ctx->expr());
    }

    Any visitIdExpr(ParaCLParser::IdExprContext* ctx) override {
        auto* node = new AST::Id();
        node->val = ctx->ID()->getText();
        return static_cast<AST::Node*>(node);
    }

    Any visitInputExpr(ParaCLParser::InputExprContext* ctx) override {
        if (!is_in_func) {
            if (ctx->input()->INT()->getText() != "0")
                throw std::runtime_error("output only takes 0 as the first arg");
            auto* node = new AST::Input();
            return static_cast<AST::Node*>(node);
        }
        auto ind = std::stoll(ctx->input()->INT()->getText());
        if (ind >= func_spec->args.size())
            throw std::runtime_error(std::to_string(ind) +
                                     " is too big number of function arg");
        auto* node = new AST::Id();
        node->val = func_spec->args[ind].first;
        return static_cast<AST::Node*>(node);
    }

    Any visitFuncExpr(ParaCLParser::FuncExprContext* ctx) override {
        auto* node = new AST::FuncBody();
        node->body = std::make_unique<AST::Block>();
        for (auto* stmtCtx : ctx->statement())
            if (auto stmt = take<AST::Statement>(visit(stmtCtx)))
                node->body->statements.push_back(std::move(stmt));

        auto& last = node->body->statements.back();
        auto ret = dynamic_cast<AST::RetStmt*>(last.get());
        auto exprstmt = dynamic_cast<AST::RetStmt*>(last.get());
        if (!ret && !exprstmt)
            throw std::runtime_error(
                "expected function to end with either ret or exprstmt!");

        if (exprstmt) {
            auto ret = std::make_unique<AST::RetStmt>();
            ret->expr = std::move(exprstmt->expr);
            node->body->statements.back() = std::move(ret);
        }
        return static_cast<AST::Node*>(node);
    }

    Any visitDotExpr(ParaCLParser::DotExprContext* ctx) override {
        auto* node = new AST::DotExpr();
        node->left = take<AST::Expr>(visit(ctx->expr()));
        node->id = ctx->ID()->getText();
        return static_cast<AST::Node*>(node);
    }

    Any visitIndexExpr(ParaCLParser::IndexExprContext* ctx) override {
        auto* node = new AST::IndexExpr();
        node->left = take<AST::Expr>(visit(ctx->expr()));
        node->ind = std::stoll(ctx->INT()->getText());
        return static_cast<AST::Node*>(node);
    }

    Any visitGlueExpr(ParaCLParser::GlueExprContext* ctx) override {
        auto* node = new AST::Glue();
        for (auto& entry : ctx->glueEntry())
            node->vals.push_back(AST::GlueEntry(entry->ID() ? entry->ID()->getText() : "",
                                                take<AST::Expr>(visit(entry->expr()))));
        return static_cast<AST::Node*>(node);
    }
};

}  // namespace ParaCompiler
