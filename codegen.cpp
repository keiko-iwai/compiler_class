#include "exprAST.h"
#include "processor.h"
using namespace llvm;
using namespace llvm::orc;

/* uncomment to print */
// #define __LOG_CODEGEN_METHODS 1
// #define __LOG_TYPECHECK 1

static void logTypecheck(const std::string &str, bool result) {
#ifdef __LOG_TYPECHECK
  std::cout << "Typecheck on " << str << ", result: " << result << std::endl;
#endif
}

static void logCodegen(const std::string &str) {
#ifdef __LOG_CODEGEN_METHODS
  std::cout << "Codegen: " << str << std::endl;
#endif
}

/* codegen methods */
Value *BlockExprAST::codeGen(CodeGenContext &context)
{
  StatementList::const_iterator it;
  Value *last = nullptr;
  for (it = Statements.begin(); it != Statements.end(); it++)
  {
    last = (**it).codeGen(context);
  }
  logCodegen("block");
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
  logCodegen("identifier reference " + Name);
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
  return Statement.codeGen(context);
}

Value *VarDeclExprAST::codeGen(CodeGenContext &context)
{
  logCodegen("variable declaration " + Name.get());
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
  logCodegen("assignment for " + LHS.Name);
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
  logCodegen("expression " + Op + ":");
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

Type *FunctionDeclarationAST::getArgumentType(CodeGenContext &context, int idx)
{
  return context.stringTypeToLLVM(Arguments[idx]->TypeName.get());
}

Value *FunctionDeclarationAST::codeGen(CodeGenContext &context)
{
  logCodegen("function " + Name.get());
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

  FunctionType *FT = FunctionType::get(context.stringTypeToLLVM(TypeName), argTypes, false);
  Function *TheFunction = Function::Create(
      FT, GlobalValue::ExternalLinkage, Name.get(), context.TheModule.get());
  context.pushFunction(TheFunction);

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

  context.popFunction();
  context.popBlock();
  context.NamesByBlock.pop_back();
  context.Builder->SetInsertPoint(context.currentBlock());
  return TheFunction;
}

Value *CallExprAST::codeGen(CodeGenContext &context)
{
  logCodegen("function call " + Name.get());
  Function *function = context.TheModule->getFunction(Name.get().c_str());
  if (!function)
  {
    std::cerr << "[AST] Function " << Name.get() << " not found" << std::endl;
    return nullptr;
  }
  FunctionDeclarationAST *fnDecl = (*context.DefinedFunctions)[Name.get()];

  std::vector<Value *> args;
  int idx = 0;
  ExpressionList::const_iterator it;
  for (it = Arguments.begin(); it != Arguments.end(); it++, idx++)
  {
    Value *val = (**it).codeGen(context);
    Type *argType = (**it).typeOf(context);
    if (fnDecl)
    {
      Type *expectedType = fnDecl->getArgumentType(context, idx);
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
  logCodegen("return");
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

Value *IfStatementAST::codeGen(CodeGenContext &context)
{
  std::cerr << "[AST] IF codegen" << std::endl;
  Value *condVal = Expr->codeGen(context);
  // create true/false comparison
  condVal = context.CreateNonZeroCmp(context.Builder, condVal, Expr->typeOf(context));

  // need a function to be defined
  Function *TheFunction = context.currentFunction(); // current function
  BasicBlock *ThenBB = BasicBlock::Create(*context.TheContext, "then", TheFunction);
  BasicBlock *ElseBB = BasicBlock::Create(*context.TheContext, "else");
  BasicBlock *MergeBB = BasicBlock::Create(*context.TheContext, "ifcont");

  // then-block
  context.Builder->SetInsertPoint(ThenBB);
  Value *IRThen = ThenBlock->codeGen(context);
  // finish then-block
  context.Builder->CreateBr(MergeBB);
  ThenBB = context.Builder->GetInsertBlock();

  // codegen for else-block:
  TheFunction->insert(TheFunction->end(), ElseBB);
  context.Builder->SetInsertPoint(ElseBB);
  Value *IRElse = ElseBlock ? ElseBlock->codeGen(context) : nullptr;
  context.Builder->CreateBr(MergeBB);

  // codegen of 'else' can change the current block, update ElseBB for the PHI.
  ElseBB = context.Builder->GetInsertBlock();
  TheFunction->insert(TheFunction->end(), MergeBB);
  context.Builder->SetInsertPoint(MergeBB);

  // result of the if-else block is a phi node
  PHINode *PN = context.Builder->CreatePHI(
    Type::getDoubleTy(*context.TheContext), 2 /*numReservedValues*/, "iftmp");

  PN->addIncoming(IRThen, ThenBB); // block value
  PN->addIncoming(IRElse, ElseBB);

  return PN;
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
  if (function)
    return context.stringTypeToLLVM(function->TypeName.get());

  Function *externalFn = context.TheModule->getFunction(name.c_str());
  if (externalFn)
    return externalFn->getReturnType();

  std::cerr << "Typecheck on call expression: function " << name << " not found" << std::endl;
  return nullptr;
}

Type *BlockExprAST::typeOf(CodeGenContext &context)
{
  StatementAST *last = Statements.back();
  return last->typeOf(context);
}

Type *ReturnStatementAST::typeOf(CodeGenContext &context)
{
  if (!Expr)
    return Type::getVoidTy(*context.TheContext);
  Type *t = Expr->typeOf(context);
  return Expr->typeOf(context);
}

bool BlockExprAST::typeCheck(CodeGenContext &context)
{
  StatementList::const_iterator it;
  bool result = true;
  for (it = Statements.begin(); it != Statements.end(); it++)
  {
    result = result && (**it).typeCheck(context);
    if (!result)
      break;
  }
  logTypecheck("block", result);
  return result;
}

bool BinaryExprAST::typeCheck(CodeGenContext &context)
{
  Type *L = LHS->typeOf(context);
  Type *R = RHS->typeOf(context);
  bool result = L == R || context.isTypeConversionPossible(L, R);
  logTypecheck("expression " + Op, result);
  return result;
}

bool AssignmentAST::typeCheck(CodeGenContext &context)
{
  Type *L = LHS.typeOf(context);
  Type *R = RHS.typeOf(context);
  bool result = L == R || context.isTypeConversionPossible(L, R);
  logTypecheck("assignment " + LHS.Name, result);
  return result;
}

bool VarDeclExprAST::typeCheck(CodeGenContext &context)
{
  bool assignmentResult = AssignmentExpr->typeCheck(context);
  if (!assignmentResult)
    return assignmentResult;

  NameTable *currentBlockTable = context.NamesByBlock.back();
  (*currentBlockTable)[Name.get()] = context.stringTypeToLLVM(TypeName);

  Type *L = context.stringTypeToLLVM(TypeName);
  Type *R = AssignmentExpr->typeOf(context);

  bool result = L == R || context.isTypeConversionPossible(L, R);
  logTypecheck("var decl " + Name.get(), result);
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
  context.NamesByBlock.pop_back();

  logTypecheck("function return type " + Name.get(), result);
  return result;
}

bool CallExprAST::typeCheck(CodeGenContext &context)
{
  FunctionDeclarationAST *function = (*context.DefinedFunctions)[Name.get()];
  if (!function) // external function
  {
    return true;
  }

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
    // the nametable of function arguments does not exist outside.
    // We obtain the text name of the type and convert to LLVM Type *
    Type *argType = function->getArgumentType(context, idx);
    if (valueType != argType && !context.isTypeConversionPossible(valueType, argType))
    {
      std::cerr << "Typecheck on function call " << Name.get() << " failed: argument type "
        << function->Arguments[idx]->Name.get() << " is wrong " << std::endl;
      return false;
    }
  }

  logTypecheck("function call " + Name.get(), true);
  return true;
}
