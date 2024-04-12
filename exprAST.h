#include <iostream>
#include <memory>
namespace {

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

class BinaryExprAST : public ExprAST {
  std::string Op;
  ExprAST *LHS, *RHS;

public:
  BinaryExprAST(std::string Op, ExprAST *LHS, ExprAST *RHS)
      : Op(Op), LHS(LHS), RHS(RHS) { std::cout << "Expression parsed " << Op << std::endl; }
};

}
