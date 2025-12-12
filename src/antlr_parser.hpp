#pragma once

#include <any>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

#include "ParaCLBaseVisitor.h"
#include "ast.hpp"

namespace ParaCompiler {

using Any = std::any;

class TreeBuilder : public ParaCLBaseVisitor {
   public:
    // Helper: Extract Raw Pointer from std::any, cast to T*, wrap in unique_ptr
    template <typename T>
    std::unique_ptr<T> take(Any a) {
        if (!a.has_value()) return nullptr;
        try {
            AST::Node* base_ptr = std::any_cast<AST::Node*>(a);
            T* typed_ptr = dynamic_cast<T*>(base_ptr);
            if (!typed_ptr && base_ptr != nullptr) {
                std::cerr << "ICE: AST type mismatch.\n";
                return nullptr;
            }
            return std::unique_ptr<T>(typed_ptr);
        } catch (...) {
            return nullptr;
        }
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
        if (ctx->assignment()) return visit(ctx->assignment());

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
        node->name = ctx->ID()->getText();
        if (ctx->typeSpec()) node->typeSpec = take<AST::TypeSpec>(visit(ctx->typeSpec()));
        if (ctx->expr()) node->val = take<AST::Expr>(visit(ctx->expr()));
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
        if (ctx->input()->INT()->getText() != "0")
            throw std::runtime_error("output only takes 0 as the first arg");
        auto* node = new AST::Input();
        return static_cast<AST::Node*>(node);
    }
};

}  // namespace ParaCompiler
