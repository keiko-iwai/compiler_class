#include "AST.h"
#include "codegen.h"
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
Value *BlockExprAST::createIR(Codegen &context, bool needPrintIR)
{
  StatementList::const_iterator it;
  Value *last = nullptr;
  for (it = Statements.begin(); it != Statements.end(); it++)
  {
    last = (**it).createIR(context, needPrintIR);
  }
  logCodegen("block");
  return last;
}

Value *FunctionBlockAST::createIR(Codegen &context, bool needPrintIR)
{
  if (Block)
    Block->createIR(context, needPrintIR);

  Value *last = ReturnStmt.createIR(context, needPrintIR);
  logCodegen("function block");
  return last;
}

Value *IntExprAST::createIR(Codegen &context, bool _needPrintIR)
{
  return ConstantInt::get(Type::getInt32Ty(*context.TheContext), Val, true /*signed*/);
}

Value *DoubleExprAST::createIR(Codegen &context, bool _needPrintIR)
{
  return ConstantFP::get(Type::getDoubleTy(*context.TheContext), Val);
}

Value *StringExprAST::createIR(Codegen &context, bool _needPrintIR)
{
  // create a string global variable; return a link to it
  auto charType = Type::getInt8Ty(*context.TheContext);

  std::vector<llvm::Constant *> chars(Val.size());
  for(int i = 0; i < Val.size(); i++) {
    chars[i] = ConstantInt::get(charType, Val[i]);
  }
  // Add \0
  chars.push_back(ConstantInt::get(charType, 0));

  auto stringType = ArrayType::get(charType, chars.size());
  std::string name = context.genStrConstantName();
  auto globalDeclaration = (GlobalVariable *) context.TheModule->getOrInsertGlobal(name, stringType);
  globalDeclaration->setInitializer(ConstantArray::get(stringType, chars));
  globalDeclaration->setConstant(true);
  globalDeclaration->setLinkage(llvm::GlobalValue::LinkageTypes::PrivateLinkage);
  globalDeclaration->setUnnamedAddr (llvm::GlobalValue::UnnamedAddr::Global);
  // Return a cast to an i8*
  return llvm::ConstantExpr::getBitCast(globalDeclaration, charType->getPointerTo());
}

Value *IdentifierExprAST::createIR(Codegen &context, bool _needPrintIR)
{
  logCodegen("identifier reference " + Name);
  CodegenBlock *TheBlock = context.GeneratingBlocks.top();
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

Value *ExpressionStatementAST::createIR(Codegen &context, bool needPrintIR)
{
  return Statement.createIR(context, needPrintIR);
}

Value *VarDeclExprAST::createIR(Codegen &context, bool needPrintIR)
{
  std::string name = Name.get();
  logCodegen("variable declaration " + name);
  CodegenBlock *TheBlock = context.GeneratingBlocks.top();
  AllocaInst *Alloca = context.createBlockAlloca(
      TheBlock->block, context.stringTypeToLLVM(TypeName), name.c_str());
  TheBlock->locals[name] = Alloca;

  NameTable *Names = context.NameTypesByBlock.back();
  (*Names)[name] = context.stringTypeToLLVM(TypeName);

  if (AssignmentExpr)
  {
    AssignmentAST assignment(Name, *AssignmentExpr);
    assignment.createIR(context, needPrintIR);
  }
  return Alloca;
}

Value *AssignmentAST::createIR(Codegen &context, bool needPrintIR)
{
  logCodegen("assignment for " + LHS.Name);
  CodegenBlock *TheBlock = context.GeneratingBlocks.top();

  AllocaInst *Alloca = TheBlock->locals[LHS.Name];
  if (!Alloca)
  {
    std::cerr << "[AST] Undeclared variable " << LHS.Name << std::endl;
    return nullptr;
  }

  Value *value = RHS.createIR(context, needPrintIR);
  llvm::Type *resultType = Alloca->getAllocatedType();
  if (RHS.typeOf(context) != resultType)
  {
    value = context.createTypeCast(context.Builder, value, resultType);
  }

  if (!value)
  {
    std::cerr << "[AST] Not generated value for " << LHS.Name << std::endl;
    return nullptr;
  }
  return context.Builder->CreateStore(value, Alloca);
}

Value *BinaryExprAST::createIR(Codegen &context, bool needPrintIR)
{
  /* integers are i32 and signed */
  logCodegen("expression " + Op + ":");
  Value *L = LHS->createIR(context, needPrintIR);
  Value *R = RHS->createIR(context, needPrintIR);
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
      L = context.createTypeCast(context.Builder, L, doubleType);
    if (Rtype != doubleType)
      R = context.createTypeCast(context.Builder, R, doubleType);
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


Value *UnaryExprAST::createIR(Codegen &context, bool needPrintIR)
{
  logCodegen("expression " + Op + ":");
  Value *Val = Expr->createIR(context, needPrintIR);
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

llvm::Type *FunctionDeclarationAST::getArgumentType(Codegen &context, int idx)
{
  return context.stringTypeToLLVM(Arguments[idx]->TypeName.get());
}

Value *FunctionDeclarationAST::createIR(Codegen &context, bool needPrintIR)
{
  logCodegen("function " + Name.get());
  // the type casts refer to the name table
  NameTable *Names = new NameTable();
  context.NameTypesByBlock.push_back(Names);
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

  CodegenBlock *TheBlock = context.GeneratingBlocks.top();
  TheBlock->locals.clear();

  unsigned idx = 0;
  for (auto &Arg : TheFunction->args())
  {
    std::string name = Arguments[idx]->Name.get();
    AllocaInst *Alloca = context.createBlockAlloca(
        TheBlock->block, argTypes[idx], name);

    Arg.setName(name);
    context.Builder->CreateStore(&Arg, Alloca);
    TheBlock->locals[name] = Alloca;
    idx++;
  }

  Value *RetVal = Block.createIR(context, needPrintIR);
  llvm::Type *returnType = context.stringTypeToLLVM(TypeName);
  llvm::Type *blockType = Block.typeOf(context);
  if (blockType != returnType && context.isTypeConversionPossible(blockType, returnType)) {
    RetVal = context.createTypeCast(context.Builder, RetVal, returnType);
  }

  verifyFunction(*TheFunction);

  if (needPrintIR)
    TheFunction->print(*context.out);

  context.popFunction();
  context.popBlock();
  context.NameTypesByBlock.pop_back();
  if (!context.GeneratingBlocks.empty()) // stack is empty when we exit the main function
    context.Builder->SetInsertPoint(context.currentBlock());
  return TheFunction;
}

Value *CallExprAST::createIR(Codegen &context, bool needPrintIR)
{
  logCodegen("function call " + Name.get());
  Function *function = context.TheModule->getFunction(Name.get().c_str());
  if (!function)
  {
    std::cerr << "[AST] Function " << Name.get() << " not found" << std::endl;
    return nullptr;
  }
  FunctionType *fnType = function->getFunctionType();

  std::vector<Value *> args;
  int idx = 0;
  ExpressionList::const_iterator it;
  bool isVariadic = fnType->isVarArg();
  int numParams = fnType->getNumParams();
  for (it = Arguments.begin(); it != Arguments.end(); it++, idx++)
  {
    Value *val = (**it).createIR(context, needPrintIR);
    if (isVariadic && idx >= numParams)
    {
      args.push_back(val); // no conversion
      continue;
    }
    llvm::Type *argType = (**it).typeOf(context);
    llvm::Type *expectedType = fnType->getParamType(idx);
    if (argType != expectedType)
    {
      if (!context.isTypeConversionPossible(argType, expectedType)) {
        std::cout << "[AST] incompatible argument type " << idx << " for function" << Name.get() << std::endl;
        return nullptr;
      }
      else
        val = context.createTypeCast(context.Builder, val, expectedType);
    }
    args.push_back(val);
  }
  CallInst *call = context.Builder->CreateCall(function, args, Name.get());
  return call;
}

Value *ReturnStatementAST::createIR(Codegen &context, bool needPrintIR)
{
  logCodegen("return");
  if (!Expr)
  {
    context.Builder->CreateRetVoid();
    return nullptr;
  }

  Function *function = context.currentFunction(); // current function
  FunctionType *fnType = function->getFunctionType();
  llvm::Type *expectedType = fnType->getReturnType();
  std::string fnName = std::string(function->getName());

  Value *RetVal = Expr->createIR(context, needPrintIR);
  if (!RetVal)
  {
    std::cerr << "[AST] Failed to generate return result " << fnName << std::endl;
    return nullptr;
  }

  llvm::Type *valueType = RetVal->getType();
  if (valueType != expectedType) {
    if (!context.isTypeConversionPossible(valueType, expectedType))
    {
      std::cout << "[AST] Function " << fnName << ": return value type " << context.print(valueType)
        << " can not convert to expected type" << context.print(expectedType) << std::endl;
      Expr->pp();
      return nullptr;
    }
    RetVal = context.createTypeCast(context.Builder, RetVal, expectedType);
  }
  context.Builder->CreateRet(RetVal);
  return RetVal;
}

Value *IfStatementAST::createIR(Codegen &context, bool needPrintIR)
{
  logCodegen("if");
  Value *condVal = Expr->createIR(context, needPrintIR);
  // create true/false comparison
  condVal = context.createNonZeroCmp(context.Builder, condVal);

  // need a function to be defined
  Function *TheFunction = context.currentFunction(); // current function
  BasicBlock *ThenBB = BasicBlock::Create(*context.TheContext, "then", TheFunction);
  BasicBlock *ElseBB = BasicBlock::Create(*context.TheContext, "else");
  BasicBlock *MergeBB = BasicBlock::Create(*context.TheContext, "ifcont");

  context.Builder->CreateCondBr(condVal, ThenBB, ElseBB);
  // then-block
  context.Builder->SetInsertPoint(ThenBB);
  if (ThenBlock)
    ThenBlock->createIR(context, needPrintIR);
  // finish then-block
  context.Builder->CreateBr(MergeBB);

  // codegen for else-block:
  TheFunction->insert(TheFunction->end(), ElseBB);
  context.Builder->SetInsertPoint(ElseBB);
  if (ElseBlock)
    ElseBlock->createIR(context, needPrintIR);
  // finish else-block
  context.Builder->CreateBr(MergeBB);

  TheFunction->insert(TheFunction->end(), MergeBB);
  context.Builder->SetInsertPoint(MergeBB);

  return condVal;
}

Value *ForStatementAST::createIR(Codegen &context, bool needPrintIR)
{
  logCodegen("for");
  ExpressionList::const_iterator it;
  for (it = Before.begin(); it != Before.end(); it++)
  {
    (**it).createIR(context, needPrintIR);
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
  Value *condVal = Expr->createIR(context, needPrintIR);
  condVal = context.createNonZeroCmp(context.Builder, condVal);
  context.Builder->CreateCondBr(condVal, BodyBB, ExitBB);

  TheFunction->insert(TheFunction->end(), BodyBB);
  context.Builder->SetInsertPoint(BodyBB);
  if (Block)
    Block->createIR(context, needPrintIR);

  for (it = After.begin(); it != After.end(); it++)
  {
    (**it).createIR(context, needPrintIR);
  }
  context.Builder->CreateBr(LoopBB);

  TheFunction->insert(TheFunction->end(), ExitBB);
  context.Builder->SetInsertPoint(ExitBB);

  return condVal;
}

/* typeOf methods */
llvm::Type *NodeAST::typeOf(Codegen &context)
{
  std::cout << "Default typeOf called! Possible mistake. Type of the expression="
    << typeid(this).name() << std::endl;
  return Type::getVoidTy(*context.TheContext);
}

llvm::Type *IntExprAST::typeOf(Codegen &context)
{
  return Type::getInt32Ty(*context.TheContext);
}

llvm::Type *DoubleExprAST::typeOf(Codegen &context)
{
  return Type::getDoubleTy(*context.TheContext);
}

llvm::Type *StringExprAST::typeOf(Codegen &context)
{
  return PointerType::getUnqual(Type::getInt8Ty(*context.TheContext));
}

bool ExprAST::isNumeric(Codegen &context, llvm::Type *type) {
  return type == Type::getDoubleTy(*context.TheContext) || type == Type::getInt32Ty(*context.TheContext);
}

bool ExprAST::isString(Codegen &context, llvm::Type *type) {
  return type == PointerType::getUnqual(Type::getInt8Ty(*context.TheContext));
}

llvm::Type *IdentifierExprAST::typeOf(Codegen &context)
{
  std::vector<NameTable *>::const_iterator it;
  llvm::Type *type = nullptr;
  for (it = context.NameTypesByBlock.begin(); it != context.NameTypesByBlock.end(); it++)
  {
    type = (**it)[Name];
    if (type)
      return type;
  }
  std::cerr << "[AST] typeOf: unknown variable " << Name << std::endl;
  return type;
}

llvm::Type *BinaryExprAST::typeOf(Codegen &context)
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

llvm::Type *UnaryExprAST::typeOf(Codegen &context)
{
  if (!typeCheck(context))
  {
    std::cerr << "[AST] Failed type check in expession " << Op << std::endl;
    return Type::getVoidTy(*context.TheContext);
  }
  return Expr->typeOf(context);
}

llvm::Type *ExpressionStatementAST::typeOf(Codegen &context)
{
  return Statement.typeOf(context);
}

llvm::Type *CallExprAST::typeOf(Codegen &context)
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

llvm::Type *FunctionBlockAST::typeOf(Codegen &context)
{
  return ReturnStmt.typeOf(context);
}

llvm::Type *ReturnStatementAST::typeOf(Codegen &context)
{
  if (!Expr)
    return Type::getVoidTy(*context.TheContext);
  llvm::Type *t = Expr->typeOf(context);
  return Expr->typeOf(context);
}

bool BlockExprAST::typeCheck(Codegen &context)
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

bool FunctionBlockAST::typeCheck(Codegen &context)
{
  bool result = ReturnStmt.typeCheck(context) && (!Block || Block->typeCheck(context));
  logTypecheck("function block", result);
  return result;
}

bool BinaryExprAST::typeCheck(Codegen &context)
{
  llvm::Type *L = LHS->typeOf(context);
  llvm::Type *R = RHS->typeOf(context);
  bool result = L == R || context.isTypeConversionPossible(L, R);
  logTypecheck("expression " + Op, result);
  return result;
}

bool UnaryExprAST::typeCheck(Codegen &context)
{
  llvm::Type *ExprType = Expr->typeOf(context);
  return (Op.compare("-") == 0) && isNumeric(context, ExprType);
}

bool AssignmentAST::typeCheck(Codegen &context)
{
  llvm::Type *L = LHS.typeOf(context);
  llvm::Type *R = RHS.typeOf(context);
  bool result = L == R || context.isTypeConversionPossible(L, R);
  logTypecheck("assignment " + LHS.Name, result);
  return result;
}

bool VarDeclExprAST::typeCheck(Codegen &context)
{
  NameTable *currentBlockTable = context.NameTypesByBlock.back();
  (*currentBlockTable)[Name.get()] = context.stringTypeToLLVM(TypeName);

  if (!AssignmentExpr)
    return true;

  if (!AssignmentExpr->typeCheck(context))
    return false;

  llvm::Type *L = context.stringTypeToLLVM(TypeName);
  llvm::Type *R = AssignmentExpr->typeOf(context);

  bool result = L == R || context.isTypeConversionPossible(L, R);
  logTypecheck("var decl " + Name.get(), result);
  return result;
}

bool FunctionDeclarationAST::typeCheck(Codegen &context)
{
  NameTable *Names = new NameTable();
  context.NameTypesByBlock.push_back(Names);

  VariableList::const_iterator it;
  for (it = Arguments.begin(); it != Arguments.end(); it++)
  {
    (*Names)[(**it).Name.get()] = context.stringTypeToLLVM((**it).TypeName);
  }

  bool result = Block.typeCheck(context);
  llvm::Type *FNType = context.stringTypeToLLVM(TypeName);
  llvm::Type *Ret = Block.typeOf(context);
  result = result && (FNType == Ret || context.isTypeConversionPossible(FNType, Ret));
  context.NameTypesByBlock.pop_back();

  logTypecheck("function return type " + Name.get(), result);
  return result;
}

bool CallExprAST::typeCheckExternalFn(Codegen &context, Function *function)
{
  FunctionType *fnType = function->getFunctionType();
  int numParams = fnType->getNumParams();
  bool isVariadic = fnType->isVarArg();

  if (Arguments.size() > numParams && !isVariadic)
  {
    std::cerr << "Typecheck on function call " << Name.get()
      << " failed: number of arguments is wrong." << std::endl;
    return false;
  }

  ExpressionList::const_iterator it;
  int idx = 0;
  for (it = Arguments.begin(); it != Arguments.end(); it++, idx++)
  {
    if (isVariadic && idx >= numParams)
      break; // no typecheck

    llvm::Type *valueType = (**it).typeOf(context);
    // the nametable of function arguments does not exist outside.
    // We obtain the text name of the type and convert to LLVM llvm::Type *
    llvm::Type *argType = fnType->getParamType(idx);
    if (valueType != argType && !context.isTypeConversionPossible(valueType, argType))
    {
      std::cerr << "Typecheck on function call " << Name.get()
        << " failed: argument type " << idx << " is wrong " << std::endl;
      return false;
    }
  }
  return true;
}

bool CallExprAST::typeCheckUserFn(Codegen &context)
{
  FunctionDeclarationAST *fnDecl = (*context.DefinedFunctions)[Name.get()];
  if (!fnDecl) {
    std::cerr << "[Typecheck on function call " << Name.get()
      << " failed: function not found" << std::endl;
    return false;
  }
  if (fnDecl->Arguments.size() != Arguments.size())
  {
    std::cerr << "Typecheck on function call " << Name.get()
      << " failed: number of arguments is wrong." << std::endl;
    return false;
  }

  ExpressionList::const_iterator it;
  int idx = 0;
  for (it = Arguments.begin(); it != Arguments.end(); it++, idx++)
  {
    llvm::Type *valueType = (**it).typeOf(context);
    llvm::Type *argType = fnDecl->getArgumentType(context, idx);
    if (valueType != argType && !context.isTypeConversionPossible(valueType, argType))
    {
      std::cerr << "Typecheck on function call " << Name.get() << " failed: argument type "
        << idx << " is wrong " << std::endl;
      return false;
    }
  }
  return true;
}

bool CallExprAST::typeCheck(Codegen &context)
{
  Function *function = context.TheModule->getFunction(Name.get().c_str());
  bool result = !function ? typeCheckUserFn(context)
    : typeCheckExternalFn(context, function);

  logTypecheck("function call " + Name.get(), result);
  return result;
}
