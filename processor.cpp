#include "exprAST.h"
#include "processor.h"
#include "parser.hpp"
#include "runtime.h"

using namespace llvm;
using namespace llvm::orc;

const std::string CodeGenContext::genStrConstantName()
{
  objCount ++;
  return "_str" + std::to_string(objCount);
}

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

void CodeGenContext::generateCode(BlockExprAST &mainBlock)
{
  std::cout << "Generating code...\n";

  // create main function
  IdentifierExprAST type = IdentifierExprAST("int");
  IdentifierExprAST name = IdentifierExprAST("main");
  VariableList args;
  FunctionDeclarationAST *main = new FunctionDeclarationAST(type, name, args, mainBlock);

  std::vector<llvm::Type *> Args;
  FunctionType *FT = FunctionType::get(Type::getInt32Ty(*TheContext), Args, false);
  Function *TheFunction = Function::Create(
      FT, Function::ExternalLinkage, _mainFunctionName, TheModule.get());
  pushFunction(TheFunction);

  BasicBlock *bblock = BasicBlock::Create(*TheContext, "entry", TheFunction);
  Builder->SetInsertPoint(bblock);

  // Push a new variable/block context
  pushBlock(bblock);
  Value *RetVal = mainBlock.codeGen(*this);
  Builder->CreateRetVoid();
  popBlock();
  popFunction();

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

void CodeGenContext::runCode()
{
  std::cout << "Running code...\n";
  auto RT = TheJIT->getMainJITDylib().createResourceTracker();
  auto TSM = ThreadSafeModule(std::move(TheModule), std::move(TheContext));
  ExitOnErr(TheJIT->addModule(std::move(TSM), RT));
  InitializeModuleAndManagers();

  auto ExprSymbol = ExitOnErr(TheJIT->lookup(_mainFunctionName));
  // Get the symbol's address and cast it to the right function pointer type and call it as a native function.
  int (*FP)() = ExprSymbol.getAddress().toPtr<int (*)()>();
  int result = FP();

  std::cout << std::endl << "Main function evaluated successfully to " << result << std::endl;
  ExitOnErr(RT->remove());
  return;
}

void CodeGenContext::writeObjFile(BlockExprAST &mainBlock)
{
  std::cout << "Writing object file...\n";
  TheModule = std::make_unique<Module>("_llvm_obj_module", *TheContext);
  InitializeAllTargetInfos();
  InitializeAllTargets();
  InitializeAllTargetMCs();
  InitializeAllAsmParsers();
  InitializeAllAsmPrinters();

  auto TargetTriple = sys::getDefaultTargetTriple();
  TheModule->setTargetTriple(TargetTriple);

  std::string Error;
  auto Target = TargetRegistry::lookupTarget(TargetTriple, Error);
  if (!Target) {
    errs() << Error;
    return;
  }

  auto CPU = "generic";
  auto Features = "";

  TargetOptions opt;
  auto TheTargetMachine = Target->createTargetMachine(
      TargetTriple, CPU, Features, opt, Reloc::PIC_);

  TheModule->setDataLayout(TheTargetMachine->createDataLayout());
  //AddRuntime();
  generateCode(mainBlock);

  auto Filename = "output.o";
  std::error_code EC;
  raw_fd_ostream dest(Filename, EC, sys::fs::OF_None);

  if (EC) {
    errs() << "Could not open file: " << EC.message();
    return;
  }

  legacy::PassManager pass;
  auto FileType = CodeGenFileType::ObjectFile;

  if (TheTargetMachine->addPassesToEmitFile(pass, dest, nullptr, FileType)) {
    errs() << "TheTargetMachine can't emit a file of this type";
    return;
  }

  pass.run(*TheModule);
  dest.flush();

  std::cout << "Wrote " << Filename << "\n";
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
    return std::string("int");
  if (type == Type::getDoubleTy(*TheContext))
    return std::string("double");
  if (type == Type::getVoidTy(*TheContext))
    return std::string("void");
  if (type == PointerType::getUnqual(Type::getInt8Ty(*TheContext)))
    return std::string("string");
  return std::string("unknown type");
}

/* define functions in runtime.cpp */
void CodeGenContext::AddRuntime()
{
  TheModule->getOrInsertFunction(
      "printi",
      FunctionType::get(
          Type::getInt32Ty(*TheContext),
          {Type::getInt32Ty(*TheContext)},
          false));
  TheModule->getOrInsertFunction(
      "printd",
      FunctionType::get(
          Type::getInt32Ty(*TheContext),
          {Type::getDoubleTy(*TheContext)},
          false));
  TheModule->getOrInsertFunction(
      "sqrt",
      FunctionType::get(
          Type::getDoubleTy(*TheContext),
          {Type::getDoubleTy(*TheContext)},
          false));
  TheModule->getOrInsertFunction(
      "print",
      FunctionType::get(
        Type::getInt32Ty(*TheContext),
        {Type::getInt8Ty(*TheContext)->getPointerTo()},
        true /* variadic func */
      ));
  TheModule->getOrInsertFunction(
      "println",
      FunctionType::get(
        Type::getInt32Ty(*TheContext),
        {Type::getInt8Ty(*TheContext)->getPointerTo()},
        true /* variadic func */
      ));
}
