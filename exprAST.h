#include <iostream>
#include <vector>
#include <memory>
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Type.h"

class CodeGenContext;

class NodeAST {
public:
    virtual ~NodeAST() = default;
    virtual void pp() {
        std::cout << "Default print: " << this << "\n";
    }
     virtual llvm::Value *codeGen(CodeGenContext &context) {
        std::cout << "Default codegen: " << this << "\n";
        return nullptr;
    }
    virtual llvm::Type *typeOf(CodeGenContext &context);
};

class ExprAST : public NodeAST {
/* parent of expression classes: block, int, double, identifier, method call, binary operation */
};

class StatementAST : public NodeAST {
/* parent of statements: variable declaration, function declaration, expression-statement */
};

class StatementAST;
class VarDeclExprAST;

typedef std::vector<StatementAST *> StatementList;
typedef std::vector<ExprAST *> ExpressionList;
typedef std::vector<VarDeclExprAST *> VariableList;

class BlockExprAST : public ExprAST {
public:
    StatementList statements;
    llvm::Value *codeGen(CodeGenContext &context) override;

    void pp() override {
        StatementList::const_iterator it;
        std::cout << "block: " << this << std::endl;
        for (it = statements.begin(); it != statements.end(); it++) {
            (**it).pp();
        }
    }
};

class IntExprAST : public ExprAST {
    int Val;
public:
    IntExprAST(int Val) : Val(Val) { std::cout << "Integer parsed" << std::endl; }
    llvm::Value *codeGen(CodeGenContext &context) override;

    void pp() override {
        std::cout << "int= " << Val << std::endl;
    }
    llvm::Type *typeOf(CodeGenContext &context) override;
};

class DoubleExprAST : public ExprAST {
    double Val;

public:
    DoubleExprAST(double Val) : Val(Val) { std::cout << "Double parsed" << std::endl; }
    llvm::Value *codeGen(CodeGenContext &context) override;

    void pp() override {
        std::cout << "double= " << Val << std::endl;
    }
    llvm::Type *typeOf(CodeGenContext &context) override;
};

class IdentifierExprAST : public ExprAST {
public:
    std::string Name;
    IdentifierExprAST(const std::string& Name) : Name(Name) { }
    llvm::Value *codeGen(CodeGenContext &context) override;

    void pp() override {
        std::cout << "indentifier " << Name << std::endl;
    }
    llvm::Type *typeOf(CodeGenContext &context) override;
};

class BinaryExprAST : public ExprAST {
    std::string Op;
    ExprAST *LHS, *RHS;

public:
    BinaryExprAST(std::string Op, ExprAST *LHS, ExprAST *RHS)
        : Op(Op), LHS(LHS), RHS(RHS)
        { std::cout << "Expression parsed " << Op << std::endl; }
    llvm::Value *codeGen(CodeGenContext &context) override;

    void pp() override {
        std::cout << "Expression " << Op << ":\n\tleft: ";
        LHS->pp();
        std::cout << "\tright: ";
        RHS->pp();
    }
    llvm::Type *typeOf(CodeGenContext &context) override;
};

class AssignmentAST : public ExprAST {
public:
    IdentifierExprAST &LHS;
    ExprAST &RHS;
    AssignmentAST(IdentifierExprAST &LHS, ExprAST &RHS) :
        LHS(LHS), RHS(RHS) { }
    llvm::Value *codeGen(CodeGenContext &context) override;

    void pp() override {
        std::cout << "Assignment: " << LHS.Name << " = \n\t";
        RHS.pp();
    }
};

class CallExprAST : public ExprAST {
public:
    const IdentifierExprAST &Name;
    ExpressionList Arguments;
    CallExprAST(const IdentifierExprAST &Name, ExpressionList &Arguments) :
        Name(Name), Arguments(Arguments) { }
    CallExprAST(const IdentifierExprAST& Name) : Name(Name) { }
    llvm::Value *codeGen(CodeGenContext &context) override;

    void pp() override {
        std::cout << "Call function: " << Name.Name << " = \n\t";
        ExpressionList::const_iterator it;
        for (it = Arguments.begin(); it != Arguments.end(); it++) {
            (**it).pp();
        }
    }
    llvm::Type *typeOf(CodeGenContext &context) override;
};

class ExpressionStatementAST : public StatementAST {
public:
    ExprAST &Statement;
    ExpressionStatementAST(ExprAST &Statement) : Statement(Statement) { }
    llvm::Value *codeGen(CodeGenContext &context) override;
    llvm::Type *typeOf(CodeGenContext &context) override;

    void pp() override {
        Statement.pp();
    }
};

class VarDeclExprAST : public StatementAST {
public:
    const IdentifierExprAST &TypeName;
    IdentifierExprAST &Name;
    ExprAST *AssignmentExpr;
    VarDeclExprAST(const IdentifierExprAST &TypeName, IdentifierExprAST &Name) :
        TypeName(TypeName), Name(Name), AssignmentExpr(nullptr) { }
    VarDeclExprAST(const IdentifierExprAST &TypeName, IdentifierExprAST &Name, ExprAST *AssignmentExpr) :
        TypeName(TypeName), Name(Name), AssignmentExpr(AssignmentExpr) { }
    llvm::Value *codeGen(CodeGenContext &context) override;

    void pp() override {
        std::cout << "Variable " << TypeName.Name << " " << Name.Name ;
        if (!AssignmentExpr) {
            std::cout << " unassigned" << std::endl;
        } else {
            std::cout << ":\n\t= ";
            AssignmentExpr->pp();
        }
    }
};

class FunctionDeclarationAST : public StatementAST {
public:
    const IdentifierExprAST &TypeName;
    const IdentifierExprAST &Name;
    VariableList Arguments;
    BlockExprAST &Block;
    FunctionDeclarationAST(const IdentifierExprAST &TypeName,
            const IdentifierExprAST &Name,
            const VariableList &Arguments,
            BlockExprAST &Block) :
        TypeName(TypeName), Name(Name), Arguments(Arguments), Block(Block) { }
    llvm::Value *codeGen(CodeGenContext &context) override;

    void pp() override {
        std::cout << std::endl << "Function " << TypeName.Name << " "
            << Name.Name << "\n\tArguments:";
        if (!Arguments.size()) {
            std::cout << " empty." << std::endl;
        } else {
            std::cout << std::endl;
            VariableList::const_iterator it;
            for (it = Arguments.begin(); it != Arguments.end(); it++) {
                (**it).pp();
            }
        }
        std::cout << "\tBody:" << std::endl;
        Block.pp();
        std::cout << "Function " << TypeName.Name << " end. " << std::endl;
    }
};
