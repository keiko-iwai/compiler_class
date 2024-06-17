#include <iostream>
#include <vector>
#include <map>
#include <memory>
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Function.h"

class Codegen;

class NodeAST
{
public:
  virtual ~NodeAST() = default;
  virtual bool typeCheck(Codegen &context) { return true; };
  virtual void pp()
  {
    std::cout << "Default print: " << this << "\n";
  }
  virtual llvm::Value *createIR(Codegen &context, bool needPrintIR = false)
  {
    std::cout << "Default codegen: " << this << "\n";
    return nullptr;
  }
  virtual llvm::Type *typeOf(Codegen &context);
};

class ExprAST : public NodeAST
{
  /* parent of expression classes: block, int, double, identifier, method call, binary operation */
  public:
  bool isNumeric(Codegen &context, llvm::Type *type);
  bool isString(Codegen &context, llvm::Type *type);
  const char *DataBuf; /* exposed to use in external functions */
};

class StatementAST : public NodeAST
{
  /* parent of statements: variable declaration, function declaration, expression-statement */
};

class StatementAST;
class VarDeclExprAST;
class FunctionDeclarationAST;

typedef std::vector<StatementAST *> StatementList;
typedef std::vector<ExprAST *> ExpressionList;
typedef std::vector<VarDeclExprAST *> VariableList;
typedef std::map<std::string, FunctionDeclarationAST *> FunctionMap;

class BlockExprAST : public ExprAST
{
public:
  StatementList Statements;
  llvm::Value *createIR(Codegen &context, bool needPrintIR = false) override;
  llvm::Type *typeOf(Codegen &context) override;

  void pp() override
  {
    StatementList::const_iterator it;
    std::cout << "block: " << this << std::endl;
    for (it = Statements.begin(); it != Statements.end(); it++)
    {
      (**it).pp();
    }
  }
  bool typeCheck(Codegen &context) override;
};

class IntExprAST : public ExprAST
{
  int Val;

public:
  IntExprAST(int Val) : Val(Val) {}
  llvm::Value *createIR(Codegen &context, bool needPrintIR = false) override;

  void pp() override
  {
    std::cout << "int= " << Val << std::endl;
  }
  llvm::Type *typeOf(Codegen &context) override;
};

class DoubleExprAST : public ExprAST
{
  double Val;

public:
  DoubleExprAST(double Val) : Val(Val) {}
  llvm::Value *createIR(Codegen &context, bool needPrintIR = false) override;

  void pp() override
  {
    std::cout << "double= " << Val << std::endl;
  }
  llvm::Type *typeOf(Codegen &context) override;
};

class StringExprAST : public ExprAST
{
  std::string Val;

public:
  StringExprAST(const std::string &Val) : Val(Val) {}
  llvm::Value *createIR(Codegen &context, bool needPrintIR = false) override;
  llvm::Type *typeOf(Codegen &context) override;

  void pp() override
  {
    std::cout << "string= " << Val << std::endl;
  }
};

class IdentifierExprAST : public ExprAST
{
public:
  std::string Name;
  IdentifierExprAST(const std::string &Name) : Name(Name) {}
  llvm::Value *createIR(Codegen &context, bool needPrintIR = false) override;

  void pp() override
  {
    std::cout << "indentifier " << Name << std::endl;
  }
  llvm::Type *typeOf(Codegen &context) override;
  const std::string &get() const { return Name; };
};

class BinaryExprAST : public ExprAST
{
  std::string Op;
  ExprAST *LHS, *RHS;

public:
  BinaryExprAST(std::string Op, ExprAST *LHS, ExprAST *RHS)
      : Op(Op), LHS(LHS), RHS(RHS) {}
  llvm::Value *createIR(Codegen &context, bool needPrintIR = false) override;
  bool typeCheck(Codegen &context) override;

  void pp() override
  {
    std::cout << "Expression " << Op << ":\n\tleft: ";
    LHS->pp();
    std::cout << "\tright: ";
    RHS->pp();
  }
  llvm::Type *typeOf(Codegen &context) override;
};

class UnaryExprAST : public ExprAST
{
  std::string Op;
  ExprAST *Expr;
public:
  UnaryExprAST(std::string Op, ExprAST *Expr) : Op(Op), Expr(Expr) {}
  llvm::Value *createIR(Codegen &context, bool needPrintIR = false) override;
  bool typeCheck(Codegen &context) override;
  llvm::Type *typeOf(Codegen &context) override;
};

class AssignmentAST : public ExprAST
{
public:
  IdentifierExprAST &LHS;
  ExprAST &RHS;
  AssignmentAST(IdentifierExprAST &LHS, ExprAST &RHS) : LHS(LHS), RHS(RHS) {}
  llvm::Value *createIR(Codegen &context, bool needPrintIR = false) override;
  bool typeCheck(Codegen &context) override;

  void pp() override
  {
    std::cout << "Assignment: " << LHS.Name << " = \n\t";
    RHS.pp();
  }
};

class CallExprAST : public ExprAST
{
private:
  bool typeCheckExternalFn(Codegen &context, llvm::Function *function);
  bool typeCheckUserFn(Codegen &context);

public:
  const IdentifierExprAST &Name;
  ExpressionList Arguments;
  CallExprAST(const IdentifierExprAST &Name, ExpressionList &Arguments) : Name(Name), Arguments(Arguments) {}
  CallExprAST(const IdentifierExprAST &Name) : Name(Name) {}
  llvm::Value *createIR(Codegen &context, bool needPrintIR = false) override;
  bool typeCheck(Codegen &context) override;

  void pp() override
  {
    std::cout << "Call function: " << Name.get() << " = \n\t";
    ExpressionList::const_iterator it;
    for (it = Arguments.begin(); it != Arguments.end(); it++)
    {
      (**it).pp();
    }
  }
  llvm::Type *typeOf(Codegen &context) override;
};

class ExpressionStatementAST : public StatementAST
{
public:
  ExprAST &Statement;
  ExpressionStatementAST(ExprAST &Statement) : Statement(Statement) {}
  llvm::Value *createIR(Codegen &context, bool needPrintIR = false) override;
  llvm::Type *typeOf(Codegen &context) override;

  void pp() override
  {
    Statement.pp();
  }
};

class VarDeclExprAST : public StatementAST
{
public:
  const IdentifierExprAST &TypeName;
  IdentifierExprAST &Name;
  ExprAST *AssignmentExpr;
  VarDeclExprAST(const IdentifierExprAST &TypeName, IdentifierExprAST &Name) :
    TypeName(TypeName), Name(Name), AssignmentExpr(nullptr) {}
  VarDeclExprAST(const IdentifierExprAST &TypeName, IdentifierExprAST &Name, ExprAST *AssignmentExpr):
    TypeName(TypeName), Name(Name), AssignmentExpr(AssignmentExpr) {}
  llvm::Value *createIR(Codegen &context, bool needPrintIR = false) override;
  bool typeCheck(Codegen &context) override;

  void pp() override
  {
    std::cout << "Variable " << TypeName.Name << " " << Name.Name;
    if (!AssignmentExpr)
    {
      std::cout << " unassigned" << std::endl;
    }
    else
    {
      std::cout << ":\n\t= ";
      AssignmentExpr->pp();
    }
  }
};

class FunctionDeclarationAST : public StatementAST
{
public:
  const IdentifierExprAST &TypeName;
  const IdentifierExprAST &Name;
  VariableList Arguments;
  BlockExprAST &Block;
  FunctionDeclarationAST(const IdentifierExprAST &TypeName,
                         const IdentifierExprAST &Name,
                         const VariableList &Arguments,
                         BlockExprAST &Block) : TypeName(TypeName), Name(Name), Arguments(Arguments), Block(Block) {}
  llvm::Value *createIR(Codegen &context, bool needPrintIR = false) override;
  bool typeCheck(Codegen &context) override;
  llvm::Type *getArgumentType(Codegen &context, int idx);

  void pp() override
  {
    std::cout << std::endl
              << "Function " << TypeName.Name << " "
              << Name.Name << "\n\tArguments:";
    if (!Arguments.size())
    {
      std::cout << " empty." << std::endl;
    }
    else
    {
      std::cout << std::endl;
      VariableList::const_iterator it;
      for (it = Arguments.begin(); it != Arguments.end(); it++)
      {
        (**it).pp();
      }
    }
    std::cout << "\tBody:" << std::endl;
    Block.pp();
    std::cout << "Function " << TypeName.Name << " end. " << std::endl;
  }
};

class ReturnStatementAST : public StatementAST
{
  ExprAST *Expr;

public:
  ReturnStatementAST() : Expr(nullptr) {}
  ReturnStatementAST(ExprAST *Expr) : Expr(Expr) {}
  llvm::Value *createIR(Codegen &context, bool needPrintIR = false) override;
  llvm::Type *typeOf(Codegen &context) override;

  void pp() override
  {
    std::cout << "Return: ";
    if (!Expr)
    {
      std::cout << " void" << std::endl;
    }
    else
    {
      std::cout << ":\n\t= ";
      Expr->pp();
    }
  }
};

class IfStatementAST : public StatementAST
{
public:
  ExprAST *Expr;
  BlockExprAST *ThenBlock;
  BlockExprAST *ElseBlock;

  IfStatementAST(ExprAST *Expr, BlockExprAST *ThenBlock) : Expr(Expr), ThenBlock(ThenBlock) {}
  IfStatementAST(ExprAST *Expr, BlockExprAST *ThenBlock, BlockExprAST *ElseBlock) :
    Expr(Expr), ThenBlock(ThenBlock), ElseBlock(ElseBlock) {}

  IfStatementAST(ExprAST *Expr, BlockExprAST *ThenBlock, StatementAST *Elif) :
    Expr(Expr), ThenBlock(ThenBlock) {
      ElseBlock = new BlockExprAST();
      ElseBlock->Statements.push_back(Elif);
    }
  llvm::Value *createIR(Codegen &context, bool needPrintIR = false) override;
  // no return type

  void pp() override
  {
    std::cout << "If: \n";
    Expr->pp();
    ThenBlock->pp();
    std::cout << "Else: \n";
    if (ElseBlock)
    {
      ElseBlock->pp();
    }
  }
};

class ForStatementAST : public StatementAST
{
  public:
  ExprAST *Expr;
  ExpressionList Before;
  ExpressionList After;
  BlockExprAST *Block;

  ForStatementAST(ExpressionList &Before, ExprAST *Expr, ExpressionList &After, BlockExprAST *Block)
    : Before(Before), Expr(Expr), After(After), Block(Block) {}
  llvm::Value *createIR(Codegen &context, bool needPrintIR = false) override;
  // no return type

  void pp() override
  {
    std::cout << "For loop. Before: \n";
    ExpressionList::const_iterator it;
    for (it = Before.begin(); it != Before.end(); it++)
    {
      (**it).pp();
    }
    std::cout << "Condition: \n";
    if (Expr)
    {
      Expr->pp();
    }
    std::cout << "Loop block: \n";
    if (Block)
    {
      Block->pp();
    }
    std::cout << "For loop. After: \n";
    for (it = After.begin(); it != After.end(); it++)
    {
      (**it).pp();
    }
  }
};