#include <iostream>
#include <vector>
#include <memory>

class NodeAST {
public:
    virtual ~NodeAST() = default;
    virtual void pp() {
        std::cout << "Default print: " << this << "\n";
    };
};

class ExprAST : public NodeAST {
/* parent of expression classes: block, int, double, identifier, method call, binary operation */
};

class StatementAST : public NodeAST {
/* parent of statements: variable declaration, function declaration, expression-statement */
};

typedef std::vector<StatementAST *> StatementList;

class BlockExprAST : public ExprAST {
public:
    StatementList statements;

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

    void pp() override {
        std::cout << "int= " << Val << std::endl;
    }
};

class DoubleExprAST : public ExprAST {
    double Val;

public:
    DoubleExprAST(double Val) : Val(Val) { std::cout << "Double parsed" << std::endl; }
    void pp() override {
        std::cout << "double= " << Val << std::endl;
    }
};

class IdentifierExprAST : public ExprAST {
public:
    std::string Name;
    IdentifierExprAST(const std::string& Name) : Name(Name) { }
    void pp() override {
        std::cout << "indentifier " << Name << std::endl;
    }
};

class BinaryExprAST : public ExprAST {
    std::string Op;
    ExprAST *LHS, *RHS;

public:
    BinaryExprAST(std::string Op, ExprAST *LHS, ExprAST *RHS)
        : Op(Op), LHS(LHS), RHS(RHS) { std::cout << "Expression parsed " << Op << std::endl; }
    void pp() override {
        std::cout << "Expression " << Op << ":\n\tleft: ";
        LHS->pp();
        std::cout << "\tright: ";
        RHS->pp();
    }
};

class AssignmentAST : public ExprAST {
public:
    IdentifierExprAST &LHS;
    ExprAST &RHS;
    AssignmentAST(IdentifierExprAST &LHS, ExprAST &RHS) :
        LHS(LHS), RHS(RHS) { }
    void pp() override {
        std::cout << "Assignment: " << LHS.Name << " = \n\t";
        RHS.pp();
    }
};

class ExpressionStatementAST : public StatementAST {
public:
    ExprAST &Statement;
    ExpressionStatementAST(ExprAST &Statement) :
        Statement(Statement) { }
    void pp() override {
        Statement.pp();
    }
};

class VarDeclExprAST : public StatementAST {
public:
    const IdentifierExprAST &Type;
    IdentifierExprAST &Name;
    ExprAST *AssignmentExpr;
    VarDeclExprAST(const IdentifierExprAST &Type, IdentifierExprAST &Name) :
        Type(Type), Name(Name), AssignmentExpr(nullptr) { }
    VarDeclExprAST(const IdentifierExprAST &Type, IdentifierExprAST &Name, ExprAST *AssignmentExpr) :
        Type(Type), Name(Name), AssignmentExpr(AssignmentExpr) { }
    void pp() override {
        std::cout << "Variable " << Type.Name << " " << Name.Name ;
        if (!AssignmentExpr) {
            std::cout << " unassigned" << std::endl;
        } else {
            std::cout << ":\n\t= ";
            AssignmentExpr->pp();
        }
    }
};
