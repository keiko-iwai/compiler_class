#include <iostream>

class ExprAST {
public:
    virtual ~ExprAST() = default;
};

class StatementExprAST : public ExprAST {

};

class IntExprAST : public ExprAST {
    int Val;

public:
    IntExprAST(int Val) : Val(Val) {}
};