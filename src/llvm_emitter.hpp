#include <llvm-21/llvm/IR/CmpPredicate.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>
#include <misc/Interval.h>

#include <cassert>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <utility>

#include "ast.hpp"
#include "default_visitor.hpp"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"

namespace ParaCompiler::LLVMEmitter {

struct LLVMEmitterVisitor : public Visitor::DefaultVisitor {
    using DefaultVisitor::visit;

    std::unordered_map<Symbols::Symbol *, llvm::AllocaInst *> symbols;

    llvm::LLVMContext ctx;
    llvm::IRBuilder<> builder;
    llvm::Module module;
    llvm::Function *func;
    // llvm::Type *int32Type;
    llvm::Value *last_value = nullptr;

    llvm::Value *get_last_value() {
        // to avoid using the same value twice
        return std::exchange(last_value, nullptr);
    }

    llvm::AllocaInst *create_entry_block_alloca(llvm::Function *func, llvm::Type *type,
                                                std::string_view name = "") {
        llvm::IRBuilder<> tmp_builder(&func->getEntryBlock(),
                                      func->getEntryBlock().begin());
        return tmp_builder.CreateAlloca(type, nullptr, name);
    }

    LLVMEmitterVisitor() : ctx(), builder(ctx), module("top", ctx), func(nullptr) {
        llvm::FunctionType *inputFuncType =
            llvm::FunctionType::get(llvm::Type::getInt32Ty(ctx), {}, false);
        module.getOrInsertFunction("inputFunc", inputFuncType);

        llvm::FunctionType *outputFuncType = llvm::FunctionType::get(
            llvm::Type::getVoidTy(ctx), {llvm::Type::getInt32Ty(ctx)}, false);
        module.getOrInsertFunction("outputFunc", outputFuncType);

        func = llvm::Function::Create(
            llvm::FunctionType::get(llvm::Type::getVoidTy(ctx), {}),
            llvm::Function::ExternalLinkage, "main", module);

        llvm::BasicBlock *entryBB = llvm::BasicBlock::Create(ctx, "entry", func);
        builder.SetInsertPoint(entryBB);
    }

    llvm::AllocaInst *create_alloca(Symbols::Symbol *sym, std::string_view name) {
        llvm::Function *parent_func = builder.GetInsertBlock()->getParent();
        llvm::AllocaInst *alloca =
            create_entry_block_alloca(parent_func, llvm::Type::getInt32Ty(ctx), name);

        symbols.emplace(sym, alloca);
        return alloca;
    }

    void visit(AST::Assignment &node) override {
        llvm::AllocaInst *alloca = nullptr;
        if (!symbols.contains(node.sym))
            alloca = create_alloca(node.sym, node.name);
        else
            alloca = symbols[node.sym];

        if (node.val) {
            node.val->accept(*this);
            builder.CreateStore(get_last_value(), alloca);
        }
    }

    void visit(AST::UnaryExpr &node) override {
        if (node.op != '-')
            throw std::runtime_error(std::string("unknown unary op: ") + node.op);
        node.expr->accept(*this);
        last_value = builder.CreateNeg(get_last_value());
    }

    void visit(AST::BinExpr &node) override {
        node.left->accept(*this);
        auto left = get_last_value();
        node.right->accept(*this);
        auto right = get_last_value();

        if (node.op == "+")
            last_value = builder.CreateAdd(left, right);
        else if (node.op == "-")
            last_value = builder.CreateSub(left, right);
        else if (node.op == "*")
            last_value = builder.CreateMul(left, right);
        else if (node.op == "/")
            last_value = builder.CreateSDiv(left, right);  // TODO: is it right op?
        // TODO: is logical operation optimization needed?
        // also need to change it to logical and once typing would be done
        else if (node.op == "&&")
            last_value = builder.CreateAnd(left, right);
        else if (node.op == "||")
            last_value = builder.CreateOr(left, right);
        else {
            std::unordered_map<std::string, llvm::CmpPredicate> cmps = {
                {"<=", llvm::ICmpInst::ICMP_SLE}, {"<", llvm::ICmpInst::ICMP_SLT},
                {">=", llvm::ICmpInst::ICMP_SGE}, {">", llvm::ICmpInst::ICMP_SGT},
                {"!=", llvm::ICmpInst::ICMP_NE},  {"==", llvm::ICmpInst::ICMP_EQ},
            };
            if (auto it = cmps.find(node.op); it != cmps.end())
                // TODO: remove sext once typing is done
                last_value =
                    builder.CreateSExt(builder.CreateICmp(it->second, left, right),
                                       llvm::Type::getInt32Ty(ctx));
            else
                throw std::runtime_error("unknown binop " + node.op);
        }
    }

    void visit(AST::Id &node) override {
        last_value = builder.CreateLoad(llvm::Type::getInt32Ty(ctx), symbols[node.sym]);
    }

    void visit(AST::IntLit &node) override { last_value = builder.getInt32(node.val); }

    void visit(AST::Input &node) override {
        llvm::Function *func = module.getFunction("inputFunc");
        assert(func);
        last_value = builder.CreateCall(func, {});
    }

    void visit(AST::Print &node) override {
        llvm::Function *func = module.getFunction("outputFunc");
        assert(func);
        node.expr->accept(*this);
        last_value = builder.CreateCall(func, {get_last_value()});
    }
};

}  // namespace ParaCompiler::LLVMEmitter
