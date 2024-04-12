#include <iostream>

class ExprAST {
public:
    virtual ~ExprAST() = default;
};

class IntExprAST : public ExprAST {
    int Val;

public:
    IntExprAST(int Val) : Val(Val) { std::cout << "Integer parsed" << std::endl; }
};

class DoubleExprAST : public ExprAST {
    double Val;

public:
    DoubleExprAST(double Val) : Val(Val) { std::cout << "Double parsed" << std::endl; }
};
