export module ParaCompiler:Visitor;

export namespace ParaCompiler::AST {
struct Node;
struct Statement;
struct Program;
struct Expr;
struct BinExpr;
struct UnaryExpr;
struct IntLit;
struct Id;
struct Input;
struct TypeSpec;
struct Assignment;
struct Print;
struct ExprStmt;
struct Block;
struct ForStmt;
struct WhileStmt;
struct IfStmt;
struct Conversion;
struct Glue;
struct DotExpr;
struct LValToRVal;
struct IndexExpr;
}  // namespace ParaCompiler::AST

export namespace ParaCompiler::Visitor {

struct Visitor {
    Visitor() = default;
    virtual ~Visitor() = default;

    virtual void visit(AST::Node &) = 0;
    virtual void visit(AST::Statement &) = 0;
    virtual void visit(AST::Program &) = 0;
    virtual void visit(AST::TypeSpec &) = 0;

    virtual void visit(AST::Expr &) = 0;
    virtual void visit(AST::BinExpr &) = 0;
    virtual void visit(AST::UnaryExpr &) = 0;
    virtual void visit(AST::IntLit &) = 0;
    virtual void visit(AST::Id &) = 0;
    virtual void visit(AST::Input &) = 0;
    virtual void visit(AST::Conversion &) = 0;
    virtual void visit(AST::Glue &) = 0;
    virtual void visit(AST::DotExpr &) = 0;
    virtual void visit(AST::LValToRVal &) = 0;
    virtual void visit(AST::IndexExpr &) = 0;

    virtual void visit(AST::Assignment &) = 0;
    virtual void visit(AST::Print &) = 0;
    virtual void visit(AST::ExprStmt &) = 0;
    virtual void visit(AST::Block &) = 0;
    virtual void visit(AST::ForStmt &) = 0;
    virtual void visit(AST::WhileStmt &) = 0;
    virtual void visit(AST::IfStmt &) = 0;
};

}  // namespace ParaCompiler::Visitor
