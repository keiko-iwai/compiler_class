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
        std::cerr << "Undeclared variable " << Name << std::endl;
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
        std::cerr << "Undeclared variable " << LHS.Name << std::endl;
        return nullptr;
    }
    Value *value = RHS.codeGen(context);
    if (!value)
    {
        std::cerr << "Not generated value for " << LHS.Name << std::endl;
        return nullptr;
    }
    return context.Builder->CreateStore(value, Alloca);
}

Value *BinaryExprAST::codeGen(CodeGenContext &context)
{
    Value *left = LHS->codeGen(context);
    Value *right = RHS->codeGen(context);
    if (!left || !right)
        return nullptr;
    // TODO: operations
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

    FunctionType *ftype = FunctionType::get(context.typeOf(TypeName), argTypes, false);
    Function *function = Function::Create(
        ftype, GlobalValue::ExternalLinkage, Name.Name, context.TheModule.get());

    BasicBlock *bblock = BasicBlock::Create(*context.TheContext, "entry", function);
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
        RetVal->print(errs());
    }
    else
        context.Builder->CreateRetVoid();
    verifyFunction(*function);

    context.popBlock();
    context.Builder->SetInsertPoint(context.currentBlock());
    return function;
}

Value *CallExprAST::codeGen(CodeGenContext &context)
{
    std::cout << "Creating function call: " << Name.Name << std::endl;
    Function *function = context.TheModule->getFunction(Name.Name.c_str());
    if (!function)
    {
        std::cerr << "Function " << Name.Name << " not found" << std::endl;
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
