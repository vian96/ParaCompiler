#pragma once

#include <llvm-21/llvm/IR/DerivedTypes.h>
#include <llvm/ADT/APInt.h>
#include <llvm/ADT/SmallString.h>
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
#include <llvm/Support/raw_ostream.h>

#include <cassert>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <utility>

#include "ast.hpp"
#include "default_visitor.hpp"
#include "symbol.hpp"
#include "types.hpp"

namespace ParaCompiler::LLVMEmitter {

struct LLVMEmitterVisitor : public Visitor::DefaultVisitor {
    using DefaultVisitor::visit;

    std::unordered_map<Symbols::Symbol *, llvm::AllocaInst *> symbols;
    Types::TypeManager &type_manager;

    std::unordered_map<const Types::Type *, llvm::Type *> type_to_llvm;

    llvm::LLVMContext ctx;
    llvm::IRBuilder<> builder;
    llvm::Module module;
    llvm::Function *func;
    llvm::Value *last_value = nullptr;

    void print() { module.print(llvm::outs(), nullptr); }

    llvm::Value *get_last_value() {
        if (!last_value)
            throw std::runtime_error(
                "an attempt to read nullptr result in llvm emmiter!");
        // to avoid using the same value twice
        return std::exchange(last_value, nullptr);
    }

    llvm::AllocaInst *create_entry_block_alloca(llvm::Type *type,
                                                std::string_view name = "") {
        llvm::Function *func = builder.GetInsertBlock()->getParent();
        llvm::IRBuilder<> tmp_builder(&func->getEntryBlock(),
                                      func->getEntryBlock().begin());
        return tmp_builder.CreateAlloca(type, nullptr, name);
    }

    llvm::Type *get_llvm_type(const Types::Type *type) {
        auto it = type_to_llvm.find(type);
        if (it != type_to_llvm.end()) return it->second;

        llvm::Type *res = nullptr;
        if (auto stype = dynamic_cast<const Types::StructType *>(type)) {
            std::vector<llvm::Type *> fields;
            for (auto &t : stype->fields) fields.push_back(get_llvm_type(t));
            res = llvm::StructType::create(ctx, fields);
        } else {
            try {
                auto width = type->get_width();
                res = builder.getIntNTy(width);
            } catch (std::runtime_error) {
                throw std::runtime_error("can't convert type [" +
                                         Types::Type::ptr_to_str(type) + "] to llvm");
            }
        }
        type_to_llvm.emplace(type, res);
        return res;
    }

    LLVMEmitterVisitor(Types::TypeManager &type_manager_)
        : type_manager(type_manager_),
          ctx(),
          builder(ctx),
          module("top", ctx),
          func(nullptr) {
        llvm::FunctionType *outft = llvm::FunctionType::get(
            llvm::Type::getVoidTy(ctx),
            {llvm::PointerType::getUnqual(ctx), llvm::Type::getInt32Ty(ctx)}, false);
        llvm::Function::Create(outft, llvm::Function::ExternalLinkage, "pcl_output_int__",
                               module);

        llvm::FunctionType *inft = llvm::FunctionType::get(
            llvm::Type::getVoidTy(ctx),
            {llvm::PointerType::getUnqual(ctx), llvm::Type::getInt32Ty(ctx)}, false);
        llvm::Function::Create(inft, llvm::Function::ExternalLinkage, "pcl_input_int__",
                               module);

        func = llvm::Function::Create(
            llvm::FunctionType::get(llvm::Type::getVoidTy(ctx), {}),
            llvm::Function::ExternalLinkage, "main", module);

        llvm::BasicBlock *entryBB = llvm::BasicBlock::Create(ctx, "entry", func);
        builder.SetInsertPoint(entryBB);
    }

    llvm::AllocaInst *create_alloca(Symbols::Symbol *sym, std::string_view name) {
        llvm::AllocaInst *alloca =
            create_entry_block_alloca(get_llvm_type(sym->type), name);

        symbols.emplace(sym, alloca);
        return alloca;
    }

    void visit(AST::Program &p) override {
        for (const auto &stmt : p.statements)
            if (stmt) stmt->accept(*this);

        if (builder.GetInsertBlock() && !builder.GetInsertBlock()->getTerminator())
            builder.CreateRetVoid();

        if (llvm::verifyModule(module, &llvm::errs())) {
            module.print(llvm::errs(), nullptr);
            throw std::runtime_error("Invalid LLVM IR generated");
        }
    }

    void visit(AST::Assignment &node) override {
        node.left->accept(*this);
        auto alloca = get_last_value();

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
                last_value = builder.CreateICmp(it->second, left, right);
            else
                throw std::runtime_error("unknown binop " + node.op);
        }
    }

    void visit(AST::Id &node) override {
        llvm::Value *alloca = nullptr;
        if (!symbols.contains(node.sym))
            alloca = create_alloca(node.sym, node.val);
        else
            alloca = symbols[node.sym];

        last_value = alloca;
    }

    void visit(AST::LValToRVal &node) override {
        node.expr->accept(*this);
        last_value = builder.CreateLoad(get_llvm_type(node.type), get_last_value());
    }

    void visit(AST::IntLit &node) override {
        last_value = builder.getIntN(node.type->get_width(), node.val);
    }

    // TODO: add dynamic cast checks to input and output nodes
    void visit(AST::Input &node) override {
        size_t bit_width = node.type->get_width();
        llvm::AllocaInst *buffer =
            create_entry_block_alloca(llvm::Type::getIntNTy(ctx, bit_width));

        // pcl_input_int__(ptr %buffer, i32 %width)
        llvm::Function *callee = module.getFunction("pcl_input_int__");
        llvm::Value *width_val =
            llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx), bit_width);
        builder.CreateCall(callee, {buffer, width_val});

        last_value = builder.CreateLoad(llvm::Type::getIntNTy(ctx, bit_width), buffer,
                                        "input_val");
    }

    void visit(AST::Print &node) override {
        size_t bit_width = node.expr->type->get_width();
        llvm::AllocaInst *buffer =
            create_entry_block_alloca(llvm::Type::getIntNTy(ctx, bit_width));

        node.expr->accept(*this);
        builder.CreateStore(get_last_value(), buffer);

        // pcl_output_int__(ptr %buffer, i32 %width)
        llvm::Function *callee = module.getFunction("pcl_output_int__");
        llvm::Value *width_val =
            llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx), bit_width);
        builder.CreateCall(callee, {buffer, width_val});
    }

    void visit(AST::IfStmt &node) override {
        node.expr->accept(*this);
        llvm::Value *cond = get_last_value();

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

    void visit(AST::Conversion &node) override {
        if (node.type == type_manager.get_flexiblet())
            throw std::runtime_error(
                "unexpected flexType in llvm emitter conversion node");
        node.expr->accept(*this);
        auto expr = get_last_value();
        if (node.type == type_manager.get_boolt()) {
            last_value = builder.CreateICmpNE(
                expr, llvm::ConstantInt::get(expr->getType(), 0), "to_bool");
            return;
        }

        // to int
        auto to_intt = dynamic_cast<const Types::IntType *>(node.type);
        assert(to_intt);
        if (node.expr->type == type_manager.get_boolt()) {
            last_value =
                builder.CreateZExt(expr, llvm::Type::getIntNTy(ctx, to_intt->width));
            return;
        }
        // both ints
        last_value = builder.CreateSExt(expr, llvm::Type::getIntNTy(ctx, to_intt->width));
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

        llvm::Value *iter_val =
            builder.CreateLoad(builder.getIntNTy(node.i_sym->type->get_width()), alloca);

        llvm::Value *cond = builder.CreateICmpNE(iter_val, limit, "loop_cond");
        builder.CreateCondBr(cond, body_bb, merge_bb);

        builder.SetInsertPoint(body_bb);
        node.body->accept(*this);

        llvm::Value *curr =
            builder.CreateLoad(builder.getIntNTy(node.i_sym->type->get_width()), alloca);
        llvm::Value *next =
            builder.CreateAdd(curr, builder.getIntN(node.i_sym->type->get_width(), 1));
        builder.CreateStore(next, alloca);

        // e.g. if block ends with return, adding 2 terminators is an error
        if (!builder.GetInsertBlock()->getTerminator()) builder.CreateBr(cond_bb);

        builder.SetInsertPoint(merge_bb);
    }

    void visit(AST::Glue &node) override {
        auto stype = get_llvm_type(node.type);
        auto temp = create_entry_block_alloca(stype);
        for (int i = 0; i < node.vals.size(); i++) {
            auto &val = node.vals[i];
            val.val->accept(*this);
            builder.CreateStore(get_last_value(),
                                builder.CreateStructGEP(stype, temp, i));
        }
        last_value = temp;
    }

    void visit(AST::DotExpr &node) override {
        node.left->accept(*this);
        last_value = builder.CreateStructGEP(get_llvm_type(node.left->type),
                                             get_last_value(), node.field_ind);
    }
    void visit(AST::IndexExpr &node) override {
        node.left->accept(*this);
        last_value = builder.CreateStructGEP(get_llvm_type(node.left->type),
                                             get_last_value(), node.ind);
    }
};

}  // namespace ParaCompiler::LLVMEmitter
