#include <iostream>
#include <vector>
#include <memory>

class ExprAST {
public:
    virtual ~ExprAST() = default;
    virtual void pp() {
        std::cout << "Default print: " << this << "\n";
    };
};

class StatementExprAST : public ExprAST {

};

typedef std::vector<StatementExprAST *> StatementList;

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
