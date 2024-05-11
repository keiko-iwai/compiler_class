#include "exprAST.h"
#include "processor.h"
#include "parser.hpp"

using namespace llvm;
using namespace llvm::orc;

AllocaInst *CodeGenContext::CreateBlockAlloca(BasicBlock *BB, llvm::Type *type, const std::string &VarName)
{
  IRBuilder<> TmpB(BB, BB->begin());
  return TmpB.CreateAlloca(type, nullptr, VarName);
}

Value *CodeGenContext::CreateTypeCast(std::unique_ptr<IRBuilder<>> const &Builder,
  Value *value, llvm::Type *toType)
{
  if (toType == Type::getInt32Ty(*TheContext)) // to int
    return Builder->CreateFPToSI(value, toType);
  if (toType == Type::getDoubleTy(*TheContext)) // to double
    return Builder->CreateSIToFP(value, toType);
  return value;
}

Value *CodeGenContext::CreateNonZeroCmp(std::unique_ptr<IRBuilder<>> const &Builder,
  Value *value)
{
  llvm::Type *type = value->getType();
  if (type == Type::getInt1Ty(*TheContext))
  {
    return value;
  }
  if (type == Type::getInt32Ty(*TheContext))
  {
    return Builder->CreateICmpNE(
      value, ConstantInt::get(Type::getInt32Ty(*TheContext), 0, true), "ifexpr");
  }
  if (type == Type::getDoubleTy(*TheContext))
    return Builder->CreateFCmpONE(
      value, ConstantFP::get(Type::getDoubleTy(*TheContext), APFloat(0.0)));
  return value;
}

/* Compile the AST into a module */
void CodeGenContext::generateCode(BlockExprAST &mainBlock)
{
  std::cout << "Generating code...\n";

  // create main function
  IdentifierExprAST type = IdentifierExprAST("int");
  IdentifierExprAST name = IdentifierExprAST("main");
  VariableList args;
  FunctionDeclarationAST *main = new FunctionDeclarationAST(type, name, args, mainBlock);

  std::vector<llvm::Type *> Args;
  FunctionType *FT = FunctionType::get(Type::getVoidTy(*TheContext), Args, false);
  Function *TheFunction = Function::Create(
      FT, Function::ExternalLinkage, _mainFunctionName, TheModule.get());
  pushFunction(TheFunction);

  BasicBlock *bblock = BasicBlock::Create(*TheContext, "entry", TheFunction);
  Builder->SetInsertPoint(bblock);

  // Push a new variable/block context
  pushBlock(bblock);
  Value *RetVal = mainBlock.codeGen(*this);
  Builder->CreateRetVoid(); // do not return data pointers to the stack
  popBlock();
  popFunction();

  // Validate the generated code, checking for consistency.
  verifyFunction(*TheFunction);

  TheFunction->print(errs());
  // Run the optimizer on the function.
  // TheFPM->run(*TheFunction, *TheFAM);
  std::cout << "Code is generated.\n";
}

void CodeGenContext::pp(BlockExprAST *block)
{
  block->pp();
}

CodeGenContext::CodeGenContext()
{
  InitializeNativeTarget();
  InitializeNativeTargetAsmPrinter();
  InitializeNativeTargetAsmParser();
  TheJIT = ExitOnErr(SimpleJIT::Create());
  InitializeModuleAndManagers();
}

void CodeGenContext::InitializeModuleAndManagers()
{
  // Open a new context and module.
  TheContext = std::make_unique<LLVMContext>();
  TheModule = std::make_unique<Module>("SimpleJIT", *TheContext);
  TheModule->setDataLayout(TheJIT->getDataLayout());
  AddRuntime();

  // Create a new builder for the module.
  Builder = std::make_unique<IRBuilder<>>(*TheContext);

  // Create new pass and analysis managers.
  TheFPM = std::make_unique<FunctionPassManager>();
  TheLAM = std::make_unique<LoopAnalysisManager>();
  TheFAM = std::make_unique<FunctionAnalysisManager>();
  TheCGAM = std::make_unique<CGSCCAnalysisManager>();
  TheMAM = std::make_unique<ModuleAnalysisManager>();
  ThePIC = std::make_unique<PassInstrumentationCallbacks>();
  TheSI = std::make_unique<StandardInstrumentations>(*TheContext,
                                                     /*DebugLogging*/ true);
  TheSI->registerCallbacks(*ThePIC, TheMAM.get());

  // Add transform passes.
  // Do simple "peephole" optimizations and bit-twiddling optzns.
  TheFPM->addPass(InstCombinePass());
  // Reassociate expressions.
  TheFPM->addPass(ReassociatePass());
  // Eliminate Common SubExpressions.
  TheFPM->addPass(GVNPass());
  // Simplify the control flow graph (deleting unreachable blocks, etc).
  TheFPM->addPass(SimplifyCFGPass());

  // Register analysis passes used in these transform passes.
  PassBuilder PB;
  PB.registerModuleAnalyses(*TheMAM);
  PB.registerFunctionAnalyses(*TheFAM);
  PB.crossRegisterProxies(*TheLAM, *TheFAM, *TheCGAM, *TheMAM);
}

/* Executes the AST by running the main function */
void CodeGenContext::runCode()
{
  std::cout << "Running code...\n";

  // execution
  auto RT = TheJIT->getMainJITDylib().createResourceTracker();

  auto TSM = ThreadSafeModule(std::move(TheModule), std::move(TheContext));
  ExitOnErr(TheJIT->addModule(std::move(TSM), RT));
  InitializeModuleAndManagers();

  // Search the JIT for the __anon_expr symbol.
  auto ExprSymbol = ExitOnErr(TheJIT->lookup(_mainFunctionName));
  // assert(ExprSymbol && "Function not found");

  // Get the symbol's address and cast it to the right type (takes no
  // arguments, returns a double) so we can call it as a native function.
  void (*FP)() = ExprSymbol.getAddress().toPtr<void (*)()>();
  FP();
  std::cout << "Evaluated successfully" << std::endl;

  // Delete the anonymous expression module from the JIT.
  ExitOnErr(RT->remove());

  std::cout << "Code was run.\n";
  return;
}

/* Returns an LLVM type based on the identifier */
llvm::Type *CodeGenContext::stringTypeToLLVM(const IdentifierExprAST &type)
{
  if (type.Name.compare("int") == 0)
    return Type::getInt32Ty(*TheContext);
  if (type.Name.compare("double") == 0)
    return Type::getDoubleTy(*TheContext);
  if (type.Name.compare("void") == 0)
    return Type::getVoidTy(*TheContext);
  if (type.Name.compare("string") == 0)
    return PointerType::getUnqual(Type::getInt8Ty(*TheContext)); /* pointer */

  std::cerr << "Unknown type: " << type.Name << std::endl;
  return Type::getVoidTy(*TheContext);
}

bool CodeGenContext::isTypeConversionPossible(llvm::Type *a, llvm::Type *b)
{
  llvm::Type *intType = Type::getInt32Ty(*TheContext);
  llvm::Type *doubleType = Type::getDoubleTy(*TheContext);
  return (a == intType && b == doubleType || a == doubleType && b == intType);
}

bool CodeGenContext::typeCheck(BlockExprAST &mainBlock)
{
  // construct the name => type table for the main block
  NameTable *Names = new NameTable();
  NamesByBlock.push_back(Names);
  bool result = mainBlock.typeCheck(*this);
  return result;
}

std::string CodeGenContext::print(llvm::Type *type)
{
if (type == Type::getInt32Ty(*TheContext))
  {
    return std::string("int");
  }
  if (type == Type::getDoubleTy(*TheContext))
  {
    return std::string("double");
  }
  if (type == Type::getVoidTy(*TheContext))
  {
    return std::string("void");
  }

  return std::string("unknown type");
}
