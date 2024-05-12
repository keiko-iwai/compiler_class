#include "exprAST.h"
#include "processor.h"
using namespace llvm;
using namespace llvm::orc;

/* uncomment to print */
// #define __LOG_CODEGEN_METHODS 1
// #define __LOG_TYPECHECK 1

// FIXME: pass the ast node as an argument and pp()
static void logTypecheck(const std::string &str, bool result) {
#ifdef __LOG_TYPECHECK
  if (result)
    return
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
  return ConstantInt::get(Type::getInt32Ty(*context.TheContext), Val, true /*signed*/);
}

Value *DoubleExprAST::codeGen(CodeGenContext &context)
{
  return ConstantFP::get(Type::getDoubleTy(*context.TheContext), Val);
}

Value *StringExprAST::codeGen(CodeGenContext &context)
{
  // fixme: this is actually a constant
  Array *StrHandle = new Array();
  StrHandle->length = Val.size();
  StrHandle->refCount = 1;
  DataBuf = Val.c_str();
  StrHandle->buf = (void *) DataBuf;
  context.AllocatedArrays.push_back(StrHandle);

  Constant *Addr = ConstantInt::get(Type::getInt64Ty(*context.TheContext), (int64_t)DataBuf);
  return ConstantExpr::getIntToPtr(Addr,
    PointerType::getUnqual(Type::getInt8Ty(*context.TheContext))); /* fixme: it's a struct pointer, what type is it? */
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
  llvm::Type *resultType = Alloca->getAllocatedType();
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
  /* integers are i32 and signed */
  logCodegen("expression " + Op + ":");
  Value *L = LHS->codeGen(context);
  Value *R = RHS->codeGen(context);
  if (!L || !R)
  {
    std::cerr << "[AST] Empty codegen for L=" << L << " and R=" << R << std::endl;
    return nullptr;
  }

  bool needCastToDouble = false;
  llvm::Type *doubleType = Type::getDoubleTy(*context.TheContext);
  llvm::Type *Ltype = LHS->typeOf(context);
  llvm::Type *Rtype = RHS->typeOf(context);
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
            : context.Builder->CreateAdd(L, R, "iaddtmp");
    return L;
  }
  if (Op.compare("-") == 0)
  {
    L = isDoubleType
            ? context.Builder->CreateFSub(L, R, "subtmp")
            : context.Builder->CreateSub(L, R, "isubtmp");
    return L;
  }
  if (Op.compare("*") == 0)
  {
    L = isDoubleType
            ? context.Builder->CreateFMul(L, R, "multmp")
            : context.Builder->CreateMul(L, R, "imultmp");
    return L;
  }
  if (Op.compare("/") == 0)
  {
    L = isDoubleType
            ? context.Builder->CreateFDiv(L, R, "divtmp")
            : context.Builder->CreateSDiv(L, R, "idivtmp");
    return L;
  }
  // comparison operators
  if (Op.compare("==") == 0)
  {
    L = isDoubleType
            ? context.Builder->CreateFCmpOEQ(L, R, "eqtmp")
            : context.Builder->CreateICmpEQ(L, R, "ieqtmp");
    return L;
  }
  if (Op.compare("!=") == 0)
  {
    L = isDoubleType
            ? context.Builder->CreateFCmpONE(L, R, "neqtmp")
            : context.Builder->CreateICmpNE(L, R, "ineqtmp");
    return L;
  }
  if (Op.compare(">") == 0)
  {
    L = isDoubleType
            ? context.Builder->CreateFCmpOGT(L, R, "gttmp")
            : context.Builder->CreateICmpSGT(L, R, "igttmp");
    return L;
  }
  if (Op.compare(">=") == 0)
  {
    L = isDoubleType
            ? context.Builder->CreateFCmpOGE(L, R, "getmp")
            : context.Builder->CreateICmpSGE(L, R, "igetmp");
    return L;
  }
  if (Op.compare("<") == 0)
  {
    L = isDoubleType
            ? context.Builder->CreateFCmpOLT(L, R, "lttmp")
            : context.Builder->CreateICmpSLT(L, R, "ilttmp");
    return L;
  }
  if (Op.compare("<=") == 0)
  {
    L = isDoubleType
            ? context.Builder->CreateFCmpOLE(L, R, "letmp")
            : context.Builder->CreateICmpSLE(L, R, "iletmp");
    return L;
  }
  std::cerr << "[AST] Operation " << Op << " not supported" << std::endl;
  return nullptr;
}


Value *UnaryExprAST::codeGen(CodeGenContext &context)
{
  logCodegen("expression " + Op + ":");
  Value *Val = Expr->codeGen(context);
  if (!Val)
  {
    std::cerr << "[AST] Empty codegen for Val=" << Expr << std::endl;
    return nullptr;
  }

  bool isDoubleType = Type::getDoubleTy(*context.TheContext) == Expr->typeOf(context);
  if (Op.compare("-") == 0)
  {
    Val = isDoubleType
            ? context.Builder->CreateFNeg(Val, "negation")
            : context.Builder->CreateNeg(Val, "negation");
    return Val;
  }
  std::cerr << "[AST] Unary operation " << Op << " is not supported" << std::endl;
  return nullptr;
}

llvm::Type *FunctionDeclarationAST::getArgumentType(CodeGenContext &context, int idx)
{
  return context.stringTypeToLLVM(Arguments[idx]->TypeName.get());
}

Value *FunctionDeclarationAST::codeGen(CodeGenContext &context)
{
  logCodegen("function " + Name.get());
  // to have the type casts work, typeOf refers the name table
  NameTable *Names = new NameTable();
  context.NamesByBlock.push_back(Names);
  std::vector<llvm::Type *> argTypes;
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
  llvm::Type *returnType = context.stringTypeToLLVM(TypeName);
  llvm::Type *blockType = Block.typeOf(context);
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
    llvm::Type *argType = (**it).typeOf(context);
    if (fnDecl)
    {
      llvm::Type *expectedType = fnDecl->getArgumentType(context, idx);
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
  logCodegen("if");
  Value *condVal = Expr->codeGen(context);
  // create true/false comparison
  condVal = context.CreateNonZeroCmp(context.Builder, condVal);

  // need a function to be defined
  Function *TheFunction = context.currentFunction(); // current function
  BasicBlock *ThenBB = BasicBlock::Create(*context.TheContext, "then", TheFunction);
  BasicBlock *ElseBB = BasicBlock::Create(*context.TheContext, "else");
  BasicBlock *MergeBB = BasicBlock::Create(*context.TheContext, "ifcont");

  context.Builder->CreateCondBr(condVal, ThenBB, ElseBB);
  // then-block
  context.Builder->SetInsertPoint(ThenBB);
  if (ThenBlock)
    ThenBlock->codeGen(context);
  // finish then-block
  context.Builder->CreateBr(MergeBB);

  // codegen for else-block:
  TheFunction->insert(TheFunction->end(), ElseBB);
  context.Builder->SetInsertPoint(ElseBB);
  if (ElseBlock)
    ElseBlock->codeGen(context);
  // finish else-block
  context.Builder->CreateBr(MergeBB);

  TheFunction->insert(TheFunction->end(), MergeBB);
  context.Builder->SetInsertPoint(MergeBB);

  return condVal;
}

Value *ForStatementAST::codeGen(CodeGenContext &context)
{
  logCodegen("for");
  ExpressionList::const_iterator it;
  for (it = Before.begin(); it != Before.end(); it++)
  {
    (**it).codeGen(context);
  }
  Function *TheFunction = context.currentFunction();
  /* before: x = 1;
     loop condition: x <= 10
     loop body: print(x);
          after: x = x + 1;
  */
  BasicBlock *LoopBB = BasicBlock::Create(*context.TheContext, "loop", TheFunction);
  BasicBlock *BodyBB = BasicBlock::Create(*context.TheContext, "blockLoop");
  BasicBlock *ExitBB = BasicBlock::Create(*context.TheContext, "afterLoop");

  context.Builder->CreateBr(LoopBB);
  context.Builder->SetInsertPoint(LoopBB);

  // FIXME: support an empty condition and generate the true value
  Value *condVal = Expr->codeGen(context);
  condVal = context.CreateNonZeroCmp(context.Builder, condVal);
  context.Builder->CreateCondBr(condVal, BodyBB, ExitBB);

  TheFunction->insert(TheFunction->end(), BodyBB);
  context.Builder->SetInsertPoint(BodyBB);
  if (Block)
    Block->codeGen(context);

  for (it = After.begin(); it != After.end(); it++)
  {
    (**it).codeGen(context);
  }
  context.Builder->CreateBr(LoopBB);

  TheFunction->insert(TheFunction->end(), ExitBB);
  context.Builder->SetInsertPoint(ExitBB);

  return condVal;
}

/* typeOf methods */
llvm::Type *NodeAST::typeOf(CodeGenContext &context)
{
  std::cout << "Default typeOf called! Possible mistake. Type of the expression="
    << typeid(this).name() << std::endl;
  return Type::getVoidTy(*context.TheContext);
}

llvm::Type *IntExprAST::typeOf(CodeGenContext &context)
{
  return Type::getInt32Ty(*context.TheContext);
}

llvm::Type *DoubleExprAST::typeOf(CodeGenContext &context)
{
  return Type::getDoubleTy(*context.TheContext);
}

llvm::Type *StringExprAST::typeOf(CodeGenContext &context)
{
  return PointerType::getUnqual(Type::getInt8Ty(*context.TheContext));
}

bool ExprAST::isNumeric(CodeGenContext &context, llvm::Type *type) {
  return type == Type::getDoubleTy(*context.TheContext) || type == Type::getInt32Ty(*context.TheContext);
}

bool ExprAST::isString(CodeGenContext &context, llvm::Type *type) {
  return type == PointerType::getUnqual(Type::getInt8Ty(*context.TheContext));
}

llvm::Type *IdentifierExprAST::typeOf(CodeGenContext &context)
{
  std::vector<NameTable *>::const_iterator it;
  llvm::Type *type = nullptr;
  for (it = context.NamesByBlock.begin(); it != context.NamesByBlock.end(); it++)
  {
    type = (**it)[Name];
    if (type)
      return type;
  }
  std::cerr << "[AST] Unknown variable " << Name << std::endl;
  return type;
}

llvm::Type *BinaryExprAST::typeOf(CodeGenContext &context)
{
  if (!typeCheck(context))
  {
    std::cerr << "[AST] Failed type check in expession " << Op << std::endl;
    return Type::getVoidTy(*context.TheContext);
  }
  llvm::Type *LType = LHS->typeOf(context);
  llvm::Type *RType = RHS->typeOf(context);
  return (LType == RType) ? LType : Type::getDoubleTy(*context.TheContext);
}

llvm::Type *UnaryExprAST::typeOf(CodeGenContext &context)
{
  if (!typeCheck(context))
  {
    std::cerr << "[AST] Failed type check in expession " << Op << std::endl;
    return Type::getVoidTy(*context.TheContext);
  }
  return Expr->typeOf(context);
}

llvm::Type *ExpressionStatementAST::typeOf(CodeGenContext &context)
{
  return Statement.typeOf(context);
}

llvm::Type *CallExprAST::typeOf(CodeGenContext &context)
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

llvm::Type *BlockExprAST::typeOf(CodeGenContext &context)
{
  StatementAST *last = Statements.back();
  return last->typeOf(context);
}

llvm::Type *ReturnStatementAST::typeOf(CodeGenContext &context)
{
  if (!Expr)
    return Type::getVoidTy(*context.TheContext);
  llvm::Type *t = Expr->typeOf(context);
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
  llvm::Type *L = LHS->typeOf(context);
  llvm::Type *R = RHS->typeOf(context);
  bool result = L == R || context.isTypeConversionPossible(L, R);
  logTypecheck("expression " + Op, result);
  return result;
}

bool UnaryExprAST::typeCheck(CodeGenContext &context)
{
  llvm::Type *ExprType = Expr->typeOf(context);
  return (Op.compare("-") == 0) && isNumeric(context, ExprType);
}

bool AssignmentAST::typeCheck(CodeGenContext &context)
{
  llvm::Type *L = LHS.typeOf(context);
  llvm::Type *R = RHS.typeOf(context);
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

  llvm::Type *L = context.stringTypeToLLVM(TypeName);
  llvm::Type *R = AssignmentExpr->typeOf(context);

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
  llvm::Type *FNType = context.stringTypeToLLVM(TypeName);
  llvm::Type *Ret = Block.typeOf(context);
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
    llvm::Type *valueType = (**it).typeOf(context);
    // the nametable of function arguments does not exist outside.
    // We obtain the text name of the type and convert to LLVM llvm::Type *
    llvm::Type *argType = function->getArgumentType(context, idx);
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
