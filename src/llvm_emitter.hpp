#include <llvm/IR/Function.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>
#include <misc/Interval.h>

#include <cassert>
#include <unordered_map>

#include "ast.hpp"
#include "default_visitor.hpp"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/ExecutionEngine/GenericValue.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"

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

    void visit(AST::VarDecl &node) override {
        llvm::Function *parent_func = builder.GetInsertBlock()->getParent();
        llvm::AllocaInst *alloca = create_entry_block_alloca(
            parent_func, llvm::Type::getInt32Ty(ctx), node.name);

        symbols.emplace(node.sym, alloca);

        if (node.val) {
            node.val->accept(*this);
            builder.CreateStore(last_value, alloca);
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
        last_value = builder.CreateCall(func, {last_value});
    }
};

}  // namespace ParaCompiler::LLVMEmitter
