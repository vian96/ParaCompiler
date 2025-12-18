#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/InstrTypes.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Verifier.h>

#include <cassert>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <utility>

#include "ast.hpp"
#include "default_visitor.hpp"

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
        if (!last_value)
            throw std::runtime_error(
                "an attempt to read nullptr result in llvm emmiter!");
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

    void visit(AST::Program &p) override {
        for (const auto &stmt : p.statements)
            if (stmt) stmt->accept(*this);

        if (builder.GetInsertBlock() && !builder.GetInsertBlock()->getTerminator())
            builder.CreateRetVoid();

        if (llvm::verifyModule(module, &llvm::errs()))
            throw std::runtime_error("Invalid LLVM IR generated");
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
                // TODO: remove zext once typing is done
                last_value =
                    builder.CreateZExt(builder.CreateICmp(it->second, left, right),
                                       llvm::Type::getInt32Ty(ctx));
            else
                throw std::runtime_error("unknown binop " + node.op);
        }
    }

    void visit(AST::Id &node) override {
        last_value =
            builder.CreateLoad(llvm::Type::getInt32Ty(ctx), symbols.at(node.sym));
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

    void visit(AST::IfStmt &node) override {
        node.expr->accept(*this);
        llvm::Value *cond = get_last_value();

        // TODO: must be done in type checker, remove once it's created
        if (cond->getType()->getIntegerBitWidth() != 1)
            cond = builder.CreateICmpNE(cond, llvm::ConstantInt::get(cond->getType(), 0),
                                        "to_bool");

        llvm::Function *func = builder.GetInsertBlock()->getParent();
        llvm::BasicBlock *then_bb = llvm::BasicBlock::Create(ctx, "if.then", func);
        llvm::BasicBlock *else_bb = llvm::BasicBlock::Create(ctx, "if.else");
        llvm::BasicBlock *merge_bb = llvm::BasicBlock::Create(ctx, "if.end");

        builder.CreateCondBr(cond, then_bb, else_bb);

        builder.SetInsertPoint(then_bb);
        node.trueb->accept(*this);

        // e.g. if block ends with return, adding 2 terminators is an error
        if (!builder.GetInsertBlock()->getTerminator()) builder.CreateBr(merge_bb);

        func->insert(func->end(), else_bb);
        builder.SetInsertPoint(else_bb);

        if (node.falseb) node.falseb->accept(*this);

        if (!builder.GetInsertBlock()->getTerminator()) builder.CreateBr(merge_bb);

        func->insert(func->end(), merge_bb);
        builder.SetInsertPoint(merge_bb);
    }

    void visit(AST::WhileStmt &node) override {
        llvm::Function *func = builder.GetInsertBlock()->getParent();

        llvm::BasicBlock *cond_bb = llvm::BasicBlock::Create(ctx, "loop.cond", func);
        llvm::BasicBlock *body_bb = llvm::BasicBlock::Create(ctx, "loop.body", func);
        llvm::BasicBlock *merge_bb = llvm::BasicBlock::Create(ctx, "loop.end", func);

        builder.CreateBr(cond_bb);

        builder.SetInsertPoint(cond_bb);
        node.expr->accept(*this);
        llvm::Value *cond = get_last_value();

        if (cond->getType()->getIntegerBitWidth() != 1)
            cond = builder.CreateICmpNE(cond, llvm::ConstantInt::get(cond->getType(), 0),
                                        "to_bool");

        builder.CreateCondBr(cond, body_bb, merge_bb);

        builder.SetInsertPoint(body_bb);
        node.body->accept(*this);

        // e.g. if block ends with return, adding 2 terminators is an error
        if (!builder.GetInsertBlock()->getTerminator()) builder.CreateBr(cond_bb);

        builder.SetInsertPoint(merge_bb);
    }

    void visit(AST::ForStmt &node) override {
        if (!node.slice.size()) throw std::runtime_error("unimplemented for-each loop");
        if (node.slice.size() != 2) throw std::runtime_error("wrong format of slice");

        llvm::AllocaInst *alloca = create_alloca(node.i_sym, node.id);
        node.slice[0]->accept(*this);
        builder.CreateStore(get_last_value(), alloca);

        llvm::Function *func = builder.GetInsertBlock()->getParent();

        llvm::BasicBlock *cond_bb = llvm::BasicBlock::Create(ctx, "for.cond", func);
        llvm::BasicBlock *body_bb = llvm::BasicBlock::Create(ctx, "for.body", func);
        llvm::BasicBlock *merge_bb = llvm::BasicBlock::Create(ctx, "for.end", func);

        builder.CreateBr(cond_bb);

        builder.SetInsertPoint(cond_bb);
        node.slice[1]->accept(*this);
        llvm::Value *limit = get_last_value();

        llvm::Value *iter_val = builder.CreateLoad(llvm::Type::getInt32Ty(ctx), alloca);

        llvm::Value *cond = builder.CreateICmpNE(iter_val, limit, "loop_cond");
        builder.CreateCondBr(cond, body_bb, merge_bb);

        builder.SetInsertPoint(body_bb);
        node.body->accept(*this);

        llvm::Value *curr = builder.CreateLoad(llvm::Type::getInt32Ty(ctx), alloca);
        llvm::Value *next = builder.CreateAdd(curr, builder.getInt32(1));
        builder.CreateStore(next, alloca);

        // e.g. if block ends with return, adding 2 terminators is an error
        if (!builder.GetInsertBlock()->getTerminator()) builder.CreateBr(cond_bb);

        builder.SetInsertPoint(merge_bb);
    }
};

}  // namespace ParaCompiler::LLVMEmitter
