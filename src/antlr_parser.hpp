#pragma once

#include <iostream>
#include <memory>
#include <string>

#include "ParaCLBaseVisitor.h"
#include "ast.hpp"

namespace ParaCompiler {

class TreeBuilder : public ParaCLBaseVisitor {
   public:
    // Helper: Extract Raw Pointer from antlrcpp::Any, cast to T*, wrap in unique_ptr
    template <typename T>
    std::unique_ptr<T> take(antlrcpp::Any a) {
        if (a.isNull()) return nullptr;
        try {
            AST::Node* base_ptr = a.as<AST::Node*>();
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
    antlrcpp::Any visitProgram(ParaCLParser::ProgramContext* ctx) override {
        auto* node = new AST::Program();
        for (auto* stmtCtx : ctx->statement())
            if (auto stmt = take<AST::Statement>(visit(stmtCtx)))
                node->statements.push_back(std::move(stmt));

        return static_cast<AST::Node*>(node);
    }

    antlrcpp::Any visitStatement(ParaCLParser::StatementContext* ctx) override {
        if (ctx->varDecl()) return visit(ctx->varDecl());

        if (ctx->assignment()) return visit(ctx->assignment());

        if (ctx->output()) return visit(ctx->output());

        if (ctx->expr()) {
            auto* node = new AST::ExprStmt();
            node->expr = take<AST::Expr>(visit(ctx->expr()));
            return static_cast<AST::Node*>(node);
        }
        return nullptr;
    }

    antlrcpp::Any visitVarDecl(ParaCLParser::VarDeclContext* ctx) override {
        auto* node = new AST::VarDecl();
        node->name = ctx->ID()->getText();
        if (ctx->typeSpec()) node->typeSpec = take<AST::TypeSpec>(visit(ctx->typeSpec()));

        node->val = take<AST::Expr>(visit(ctx->expr()));
        return static_cast<AST::Node*>(node);
    }

    antlrcpp::Any visitAssignment(ParaCLParser::AssignmentContext* ctx) override {
        auto* node = new AST::Assignment();
        node->name = ctx->ID()->getText();
        node->val = take<AST::Expr>(visit(ctx->expr()));
        return static_cast<AST::Node*>(node);
    }

    antlrcpp::Any visitOutput(ParaCLParser::OutputContext* ctx) override {
        auto* node = new AST::Print();
        node->expr = take<AST::Expr>(visit(ctx->expr()));
        return static_cast<AST::Node*>(node);
    }

    antlrcpp::Any visitTypeSpec(ParaCLParser::TypeSpecContext* ctx) override {
        auto* node = new AST::TypeSpec();
        node->name = ctx->getText();
        return static_cast<AST::Node*>(node);
    }

    antlrcpp::Any visitIntExpr(ParaCLParser::IntExprContext* ctx) override {
        auto* node = new AST::IntLit();
        try {
            node->val = std::stoll(ctx->INT()->getText());
        } catch (...) {
            std::cerr << "WARNING: number " << ctx->INT()->getText()
                      << " was not parsed\n";
            node->val = 0;
        }
        return static_cast<AST::Node*>(node);
    }

    antlrcpp::Any visitIdExpr(ParaCLParser::IdExprContext* ctx) override {
        auto* node = new AST::Id();
        node->val = ctx->ID()->getText();
        return static_cast<AST::Node*>(node);
    }

    antlrcpp::Any visitInputExpr(ParaCLParser::InputExprContext*) override {
        auto* node = new AST::Input();
        return static_cast<AST::Node*>(node);
    }
};

}  // namespace ParaCompiler
