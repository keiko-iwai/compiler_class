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

  Value *value = RHS.codeGen(context);
  Type *resultType = Alloca->getAllocatedType();
  if (RHS.typeOf(context) != resultType)
  {
    value = context.CreateTypeCast(context.Builder, value, resultType);
  }

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
  Value *L = LHS->codeGen(context);
  Value *R = RHS->codeGen(context);
  if (!L || !R)
  {
    std::cerr << "[AST] Empty codegen for L=" << L << " and R=" << R << std::endl;
    return nullptr;
  }

  bool needCastToDouble = false;
  Type *doubleType = Type::getDoubleTy(*context.TheContext);
  Type *Ltype = LHS->typeOf(context);
  Type *Rtype = RHS->typeOf(context);
  if (Ltype != Rtype)
  {
    needCastToDouble = true;
    if (Ltype != doubleType)
      L = context.CreateTypeCast(context.Builder, L, doubleType);
    if (Rtype != doubleType)
      R = context.CreateTypeCast(context.Builder, R, doubleType);
  }

  bool isDoubleType = needCastToDouble || doubleType == Ltype;
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
  // to have the type casts work, typeOf refers the name table
  NameTable *Names = new NameTable();
  context.NamesByBlock.push_back(Names);

  std::vector<Type *> argTypes;
  VariableList::const_iterator it;
  for (it = Arguments.begin(); it != Arguments.end(); it++)
  {
    // types
    argTypes.push_back(context.stringTypeToLLVM((**it).TypeName));
    // nametable
    (*Names)[(**it).Name.get()] = context.stringTypeToLLVM((**it).TypeName);
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
  Type *returnType = context.stringTypeToLLVM(TypeName);
  Type *blockType = Block.typeOf(context);
  if (blockType != returnType && context.isTypeConversionPossible(blockType, returnType)) {
    RetVal = context.CreateTypeCast(context.Builder, RetVal, returnType);
  }

  if (RetVal)
  {
    context.Builder->CreateRet(RetVal);
  }
  else
    context.Builder->CreateRetVoid();
  verifyFunction(*TheFunction);

  TheFunction->print(errs());

  context.popBlock();
  context.NamesByBlock.pop_back();
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

  FunctionDeclarationAST *fnDecl = (*context.DefinedFunctions)[Name.get()];
  int idx = 0;
  for (it = Arguments.begin(); it != Arguments.end(); it++, idx++)
  {
    Value *val = (**it).codeGen(context);
    Type *argType = (**it).typeOf(context);

    if (fnDecl)
    {
      Type *expectedType = fnDecl->Arguments[idx]->typeOf(context);
      if (argType != expectedType && context.isTypeConversionPossible(argType, expectedType))
      {
        val = context.CreateTypeCast(context.Builder, val, expectedType);
      }
    }
    args.push_back(val);
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
  std::cout << "Default typeOf called! Probably it's a mistake to get the type of the expression " << typeid(this).name() << std::endl;
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
  std::vector<NameTable *>::const_iterator it;
  Type *type = nullptr;
  for (it = context.NamesByBlock.begin(); it != context.NamesByBlock.end(); it++)
  {
    type = (**it)[Name];
    if (type)
      return type;
  }
  std::cerr << "[AST] Unknown variable " << Name << std::endl;
  return type;
}

Type *BinaryExprAST::typeOf(CodeGenContext &context)
{
  if (!typeCheck(context))
  {
    std::cerr << "[AST] Failed type check in expession " << Op << std::endl;
    return Type::getVoidTy(*context.TheContext);
  }
  Type *LType = LHS->typeOf(context);
  Type *RType = RHS->typeOf(context);
  return (LType == RType) ? LType : Type::getDoubleTy(*context.TheContext);
}

Type *ExpressionStatementAST::typeOf(CodeGenContext &context)
{
  return Statement.typeOf(context);
}

Type *CallExprAST::typeOf(CodeGenContext &context)
{
  std::string name = Name.get();
  FunctionDeclarationAST *function = (*context.DefinedFunctions)[name];
  if (!function)
  {
    std::cerr << "Typecheck on call expression: function " << name << " not found" << std::endl;
    return nullptr;
  }
  return context.stringTypeToLLVM(function->TypeName.get());
}

Type *BlockExprAST::typeOf(CodeGenContext &context)
{
  StatementAST *last = statements.back();
  return last->typeOf(context);
}

Type *ReturnStatementAST::typeOf(CodeGenContext &context)
{
  if (!Expr)
    return Type::getVoidTy(*context.TheContext);
  Type *t = Expr->typeOf(context);
  std::cout << "return " << context.print(t) << std::endl;
  return Expr->typeOf(context);
}

bool BlockExprAST::typeCheck(CodeGenContext &context)
{
  StatementList::const_iterator it;
  bool result = true;
  for (it = statements.begin(); it != statements.end(); it++)
  {
    result = result && (**it).typeCheck(context);
    if (!result)
      break;
  }
  std::cout << "Typecheck on block: " << this << " result=" << result << std::endl;
  return result;
}

bool BinaryExprAST::typeCheck(CodeGenContext &context)
{
  Type *L = LHS->typeOf(context);
  Type *R = RHS->typeOf(context);
  bool result = L == R || context.isTypeConversionPossible(L, R);
  std::cout << "Typecheck on expression " << Op << " result=" << result << std::endl;
  return result;
}

bool AssignmentAST::typeCheck(CodeGenContext &context)
{
  Type *L = LHS.typeOf(context);
  Type *R = RHS.typeOf(context);
  bool result = L == R || context.isTypeConversionPossible(L, R);
  std::cout << "Typecheck on assignment " << LHS.get() << " result=" << result << std::endl;
  return result;
}

bool VarDeclExprAST::typeCheck(CodeGenContext &context)
{
  NameTable *currentBlockTable = context.NamesByBlock.back();
  (*currentBlockTable)[Name.get()] = context.stringTypeToLLVM(TypeName);

  Type *L = context.stringTypeToLLVM(TypeName);
  Type *R = AssignmentExpr->typeOf(context);

  bool result = L == R || context.isTypeConversionPossible(L, R);
  std::cout << "Typecheck on var decl " << TypeName.get() <<
    " assigned type=" << context.print(R) << " result=" << result << std::endl;
  return result;
}

bool FunctionDeclarationAST::typeCheck(CodeGenContext &context)
{
  NameTable *Names = new NameTable();
  context.NamesByBlock.push_back(Names);

  VariableList::const_iterator it;
  for (it = Arguments.begin(); it != Arguments.end(); it++)
  {
    (*Names)[(**it).Name.get()] = context.stringTypeToLLVM((**it).TypeName);
  }

  bool result = Block.typeCheck(context);
  Type *FNType = context.stringTypeToLLVM(TypeName);
  Type *Ret = Block.typeOf(context);
  result = result && (FNType == Ret || context.isTypeConversionPossible(FNType, Ret));
  std::cout << "Typecheck on function return type " << TypeName.get()
    << " return " << context.print(Ret) << " result=" << result << std::endl;

  context.NamesByBlock.pop_back();
  return result;
}

bool CallExprAST::typeCheck(CodeGenContext &context)
{
  FunctionDeclarationAST *function = (*context.DefinedFunctions)[Name.get()];
  if (!function) // external function
    return true;

  ExpressionList::const_iterator it;
  int idx = 0;
  for (it = Arguments.begin(); it != Arguments.end(); it++, idx++)
  {
    if (!function->Arguments[idx])
    {
      std::cerr << "Typecheck on function call " << Name.get() << " failed: number of arguments is wrong." << std::endl;
      return false;
    }
    Type *valueType = (**it).typeOf(context);
    Type *argType = function->Arguments[idx]->typeOf(context);
    if (valueType != argType && !context.isTypeConversionPossible(valueType, argType))
    {
      std::cerr << "Typecheck on function call " << Name.get() << " failed: argument type"
        << function->Arguments[idx]->Name.get() << " is wrong " << std::endl;
      return false;
    }
  }

  std::cout << "Typecheck on function call " << Name.get() << " successful." << std::endl;
  return true;
}
