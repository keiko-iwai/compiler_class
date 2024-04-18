#include "exprAST.h"
#include "processor.h"
using namespace llvm;
using namespace llvm::orc;

/* codegen methods */
Value *BlockExprAST::codeGen(CodeGenContext &context)
{
  StatementList::const_iterator it;
  Value *last = nullptr;
  std::cout << "Creating block" << std::endl;

  for (it = statements.begin(); it != statements.end(); it++)
  {
    last = (**it).codeGen(context);
  }
  return last;
}

Value *IntExprAST::codeGen(CodeGenContext &context)
{
  return ConstantInt::get(Type::getInt32Ty(*context.TheContext), Val, true);
}

Value *DoubleExprAST::codeGen(CodeGenContext &context)
{
  return ConstantFP::get(Type::getDoubleTy(*context.TheContext), Val);
}

Value *IdentifierExprAST::codeGen(CodeGenContext &context)
{
  std::cout << "Creating identifier reference: " << Name << std::endl;
  CodeGenBlock *TheBlock = context._blocks.top();
  AllocaInst *Alloca = TheBlock->locals[Name];

  if (!Alloca)
  {
    Alloca = context.NamedValues[Name];
  }

  if (!Alloca)
  {
    std::cerr << "[AST] Undeclared variable " << Name << std::endl;
    return nullptr;
  }
  return context.Builder->CreateLoad(Alloca->getAllocatedType(), Alloca, Name.c_str());
}

Value *ExpressionStatementAST::codeGen(CodeGenContext &context)
{
  std::cout << "Creating expression: " << typeid(Statement).name() << std::endl;
  return Statement.codeGen(context);
}

Value *VarDeclExprAST::codeGen(CodeGenContext &context)
{
  std::cout << "Creating variable declaration " << TypeName.Name << " " << Name.get() << std::endl;
  CodeGenBlock *TheBlock = context._blocks.top();
  AllocaInst *Alloca = context.CreateBlockAlloca(
      TheBlock->block, context.stringTypeToLLVM(TypeName), Name.get().c_str());
  TheBlock->locals[Name.get()] = Alloca;

  if (AssignmentExpr)
  {
    AssignmentAST assignment(Name, *AssignmentExpr);
    assignment.codeGen(context);
  }
  return Alloca;
}

Value *AssignmentAST::codeGen(CodeGenContext &context)
{
  std::cout << "Creating assignment for " << LHS.Name << std::endl;
  CodeGenBlock *TheBlock = context._blocks.top();

  AllocaInst *Alloca = TheBlock->locals[LHS.Name];
  if (!Alloca)
  {
    std::cerr << "[AST] Undeclared variable " << LHS.Name << std::endl;
    return nullptr;
  }
  if (RHS.typeOf(context) != Alloca->getAllocatedType())
  {
    std::cerr << "[AST] The variable " << LHS.Name << " assigned with an incompatible type " << std::endl;
    return nullptr;
  }
  Value *value = RHS.codeGen(context);
  if (!value)
  {
    std::cerr << "[AST] Not generated value for " << LHS.Name << std::endl;
    return nullptr;
  }
  return context.Builder->CreateStore(value, Alloca);
}

Value *BinaryExprAST::codeGen(CodeGenContext &context)
{
  std::cout << "Codegen for expression " << Op << ":" << std::endl;
  Type *Ltype = LHS->typeOf(context);
  Type *Rtype = RHS->typeOf(context);
  if (Ltype != Rtype)
  {
    std::cerr << "[AST] Types for expression " << Op << " do not match" << std::endl;
    return nullptr;
  }

  bool isDoubleType = Type::getDoubleTy(*context.TheContext) == Ltype;

  Value *L = LHS->codeGen(context);
  Value *R = RHS->codeGen(context);
  if (!L || !R)
  {
    std::cerr << "[AST] Empty codegen for L=" << L << " and R=" << R << std::endl;
    return nullptr;
  }
  if (Op.compare("+") == 0)
  {
    L = isDoubleType
            ? context.Builder->CreateFAdd(L, R, "addtmp")
            : context.Builder->CreateAdd(L, R, "addtmp");
    return L;
  }
  if (Op.compare("-") == 0)
  {
    L = isDoubleType
            ? context.Builder->CreateFSub(L, R, "subtmp")
            : context.Builder->CreateSub(L, R, "subtmp");
    return L;
  }
  if (Op.compare("*") == 0)
  {
    L = isDoubleType
            ? context.Builder->CreateFMul(L, R, "multmp")
            : context.Builder->CreateMul(L, R, "multmp");
    return L;
  }
  if (Op.compare("/") == 0)
  {
    L = isDoubleType
            ? context.Builder->CreateFDiv(L, R, "divtmp")
            : context.Builder->CreateSDiv(L, R, "divtmp");
    return L;
  }
  // TODO: comparison operations
  std::cerr << "[AST] Operation " << Op << " not supported" << std::endl;
  return nullptr;
}

Value *FunctionDeclarationAST::codeGen(CodeGenContext &context)
{
  std::vector<Type *> argTypes;
  VariableList::const_iterator it;
  for (it = Arguments.begin(); it != Arguments.end(); it++)
  {
    argTypes.push_back(context.stringTypeToLLVM((**it).TypeName));
  }

  std::cout << "Creating function: " << Name.get() << std::endl;

  FunctionType *FT = FunctionType::get(context.stringTypeToLLVM(TypeName), argTypes, false);
  Function *TheFunction = Function::Create(
      FT, GlobalValue::ExternalLinkage, Name.get(), context.TheModule.get());

  BasicBlock *bblock = BasicBlock::Create(*context.TheContext, "entry", TheFunction);
  context.Builder->SetInsertPoint(bblock);
  context.pushBlock(bblock);

  CodeGenBlock *TheBlock = context._blocks.top();
  TheBlock->locals.clear();

  unsigned idx = 0;
  for (auto &Arg : TheFunction->args())
  {
    std::string name = Arguments[idx]->Name.get();
    AllocaInst *Alloca = context.CreateBlockAlloca(
        TheBlock->block, argTypes[idx], name);

    Arg.setName(name);
    context.Builder->CreateStore(&Arg, Alloca);
    TheBlock->locals[name] = Alloca;
    idx++;
  }

  Value *RetVal = Block.codeGen(context);
  if (RetVal)
  {
    context.Builder->CreateRet(RetVal);
  }
  else
    context.Builder->CreateRetVoid();
  verifyFunction(*TheFunction);

  TheFunction->print(errs());

  context.popBlock();
  context.Builder->SetInsertPoint(context.currentBlock());
  return TheFunction;
}

Value *CallExprAST::codeGen(CodeGenContext &context)
{
  std::cout << "Creating function call: " << Name.get() << std::endl;
  Function *function = context.TheModule->getFunction(Name.get().c_str());
  if (!function)
  {
    std::cerr << "[AST] Function " << Name.get() << " not found" << std::endl;
  }
  std::vector<Value *> args;
  ExpressionList::const_iterator it;
  for (it = Arguments.begin(); it != Arguments.end(); it++)
  {
    args.push_back((**it).codeGen(context));
  }
  CallInst *call = context.Builder->CreateCall(function, args, Name.get());
  return call;
}

Value *ReturnStatementAST::codeGen(CodeGenContext &context)
{
  std::cout << "Creating return: " << std::endl;
  if (!Expr)
  {
    context.Builder->CreateRetVoid();
    return nullptr;
  }

  Value *RetVal = Expr->codeGen(context);
  if (RetVal)
  {
    context.Builder->CreateRet(RetVal);
    return RetVal;
  }
  std::cerr << "[AST] Failed to generate return result " << std::endl;
  return nullptr;
}

/* typeOf methods */
Type *NodeAST::typeOf(CodeGenContext &context)
{
  return Type::getVoidTy(*context.TheContext);
}

Type *IntExprAST::typeOf(CodeGenContext &context)
{
  return Type::getInt32Ty(*context.TheContext);
}

Type *DoubleExprAST::typeOf(CodeGenContext &context)
{
  return Type::getDoubleTy(*context.TheContext);
}

Type *IdentifierExprAST::typeOf(CodeGenContext &context)
{
  CodeGenBlock *TheBlock = context._blocks.top();
  AllocaInst *Alloca = TheBlock->locals[Name];
  if (!Alloca)
  {
    std::cerr << "[AST] Unknown variable " << Name << std::endl;
    return nullptr;
  }
  return Alloca->getAllocatedType();
}

Type *BinaryExprAST::typeOf(CodeGenContext &context)
{
  if (!typeCheck(context))
  {
    std::cerr << "[AST] Failed type check in expession " << Op << std::endl;
    return Type::getVoidTy(*context.TheContext);
  }
  return LHS->typeOf(context);
}

Type *ExpressionStatementAST::typeOf(CodeGenContext &context)
{
  return Statement.typeOf(context);
}

Type *CallExprAST::typeOf(CodeGenContext &context)
{
  Function *function = context.TheModule->getFunction(Name.get().c_str());
  if (!function)
  {
    std::cerr << "[AST] Function " << Name.get() << " not found" << std::endl;
    return nullptr;
  }
  return function->getReturnType();
}

Type *ReturnStatementAST::typeOf(CodeGenContext &context)
{
  if (!Expr)
    return Type::getVoidTy(*context.TheContext);
  return Expr->typeOf(context);
}

bool BlockExprAST::typeCheck(CodeGenContext &context)
{
  StatementList::const_iterator it;
  std::cout << "block: " << this << std::endl;
  bool result = true;
  for (it = statements.begin(); it != statements.end(); it++)
  {
    result = result && (**it).typeCheck(context);
    if (!result)
      break;
  }
  return result;
}

bool BinaryExprAST::typeCheck(CodeGenContext &context)
{
  Type *L = LHS->typeOf(context);
  Type *R = RHS->typeOf(context);
  return L == R || context.isTypeConversionPossible(L, R);
}

bool AssignmentAST::typeCheck(CodeGenContext &context)
{
  Type *L = LHS.typeOf(context);
  Type *R = RHS.typeOf(context);
  return L == R || context.isTypeConversionPossible(L, R);
}

bool VarDeclExprAST::typeCheck(CodeGenContext &context)
{
  Type *L = context.stringTypeToLLVM(TypeName);
  Type *R = AssignmentExpr->typeOf(context);

  std::cout << TypeName.Name << " type=" << context.print(L) << " assigned type=" << context.print(R) << std::endl;
  return L == R || context.isTypeConversionPossible(L, R);
}

bool FunctionDeclarationAST::typeCheck(CodeGenContext &context)
{
  Type *L = context.stringTypeToLLVM(TypeName);
  Type *R = Block.typeOf(context);
  return L == R || context.isTypeConversionPossible(L, R);
}

bool CallExprAST::typeCheck(CodeGenContext &context)
{
  FunctionDeclarationAST *function = context.DefinedFunctions[Name.get()];
  if (!function) // external function
    return true;

  ExpressionList::const_iterator it;
  int idx = 0;
  for (it = Arguments.begin(); it != Arguments.end(); it++)
  {
    if (!function->Arguments[idx])
      return false;
    Type *valueType = (**it).typeOf(context);
    Type *argType = function->Arguments[idx]->typeOf(context);
    if (valueType != argType && !context.isTypeConversionPossible(valueType, argType))
      return false;
    idx++;
  }

  return true;
}
