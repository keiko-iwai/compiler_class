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

    if (!Alloca) {
        Alloca = context.NamedValues[Name];
    }

    if (!Alloca) {
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
    std::cout << "Creating variable declaration " << TypeName.Name << " " << Name.Name << std::endl;
    CodeGenBlock *TheBlock = context._blocks.top();
    AllocaInst *Alloca = context.CreateBlockAlloca(
        TheBlock->block, context.typeOf(TypeName), Name.Name.c_str());
    TheBlock->locals[Name.Name] = Alloca;

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
        std::cerr << "[AST] The variable " << LHS.Name  << " assigned with an incompatible type " << std::endl;
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
  if (Ltype != Rtype) {
    std::cerr << "[AST] Types for expression " << Op << " do not match" << std::endl;
    return nullptr;
  }

  bool isDoubleType = Type::getDoubleTy(*context.TheContext) == Ltype;

  Value *L = LHS->codeGen(context);
  Value *R = RHS->codeGen(context);
  if (!L || !R) {
    std::cerr << "[AST] Empty codegen for L=" << L << " and R=" << R << std::endl;
    return nullptr;
  }
  if (Op.compare("+") == 0) {
    L = isDoubleType
      ? context.Builder->CreateFAdd(L, R, "addtmp")
      : context.Builder->CreateAdd(L, R, "addtmp");
    return L;
  }
  if (Op.compare("-") == 0) {
    L = isDoubleType
      ? context.Builder->CreateFSub(L, R, "subtmp")
      : context.Builder->CreateSub(L, R, "subtmp");
    return L;
  }
  if (Op.compare("*") == 0) {
    L = isDoubleType
      ? context.Builder->CreateFMul(L, R, "multmp")
      : context.Builder->CreateMul(L, R, "multmp");
    return L;
  }
  if (Op.compare("/") == 0) {
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
        argTypes.push_back(context.typeOf((**it).TypeName));
    }
    std::cout << "Creating function: " << Name.Name << std::endl;

    FunctionType *FT = FunctionType::get(context.typeOf(TypeName), argTypes, false);
    Function *TheFunction = Function::Create(
        FT, GlobalValue::ExternalLinkage, Name.Name, context.TheModule.get());

    BasicBlock *bblock = BasicBlock::Create(*context.TheContext, "entry", TheFunction);
    context.Builder->SetInsertPoint(bblock);
    context.pushBlock(bblock);

    CodeGenBlock *TheBlock = context._blocks.top();
    TheBlock->locals.clear();
    for (it = Arguments.begin(); it != Arguments.end(); it++)
    {
        (**it).codeGen(context);
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
    std::cout << "Creating function call: " << Name.Name << std::endl;
    Function *function = context.TheModule->getFunction(Name.Name.c_str());
    if (!function)
    {
        std::cerr << "[AST] Function " << Name.Name << " not found" << std::endl;
    }
    std::vector<Value *> args;
    ExpressionList::const_iterator it;
    for (it = Arguments.begin(); it != Arguments.end(); it++)
    {
        args.push_back((**it).codeGen(context));
    }
    CallInst *call = context.Builder->CreateCall(function, args, Name.Name);
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
Type *NodeAST::typeOf(CodeGenContext &context) {
  return Type::getVoidTy(*context.TheContext);
}

Type *IntExprAST::typeOf(CodeGenContext &context) {
  return Type::getInt32Ty(*context.TheContext);
}

Type *DoubleExprAST::typeOf(CodeGenContext &context) {
  return Type::getDoubleTy(*context.TheContext);
}

Type *IdentifierExprAST::typeOf(CodeGenContext &context) {
  CodeGenBlock *TheBlock = context._blocks.top();
  AllocaInst *Alloca = TheBlock->locals[Name];
  if (!Alloca) {
    std::cerr << "[AST] Unknown variable " << Name << std::endl;
    return nullptr;
  }
  return Alloca->getAllocatedType();
}

Type *BinaryExprAST::typeOf(CodeGenContext &context) {
  Type *L = LHS->typeOf(context);
  Type *R = RHS->typeOf(context);
  if (L != R) {
    std::cerr << "[AST] Failed type check in expession " << Op << std::endl;
    return Type::getVoidTy(*context.TheContext);
  }
  return L;
}

Type *ExpressionStatementAST::typeOf(CodeGenContext &context) {
  return Statement.typeOf(context);
}

Type *CallExprAST::typeOf(CodeGenContext &context)
{
    Function *function = context.TheModule->getFunction(Name.Name.c_str());
    if (!function)
    {
        std::cerr << "[AST] Function " << Name.Name << " not found" << std::endl;
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
