#include "AST.h"
#include "codegen.h"
#include "parser.h"
#include "runtime.h"

using namespace llvm;
using namespace llvm::orc;

const std::string Codegen::genStrConstantName()
{
  ConstObjCount ++;
  return "_str" + std::to_string(ConstObjCount);
}

AllocaInst *Codegen::createBlockAlloca(BasicBlock *BB, llvm::Type *type, const std::string &VarName)
{
  IRBuilder<> TmpB(BB, BB->begin());
  return TmpB.CreateAlloca(type, nullptr, VarName);
}

Value *Codegen::createTypeCast(std::unique_ptr<IRBuilder<>> const &Builder,
  Value *value, llvm::Type *toType)
{
  if (toType == Type::getInt32Ty(*TheContext)) // to int
    return Builder->CreateFPToSI(value, toType);
  if (toType == Type::getDoubleTy(*TheContext)) // to double
    return Builder->CreateSIToFP(value, toType);
  return value;
}

Value *Codegen::createNonZeroCmp(std::unique_ptr<IRBuilder<>> const &Builder,
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

void Codegen::generateCode(BlockExprAST &parsedBlock, bool withOptimization = true,
  bool needPrintIR = false, std::string outputFile = "")
{
  ConstObjCount = 0;

  //redirect output
  if (!outputFile.empty()) {
    std::error_code EC;
    out = new raw_fd_ostream(outputFile, EC, sys::fs::OF_None);
  } else {
    out = &outs();
  }

  // create main function
  IdentifierExprAST type = IdentifierExprAST("int");
  IdentifierExprAST name = IdentifierExprAST(MainFunctionName);
  VariableList args;
  ExprAST *returnValue = new IntExprAST(0);
  ReturnStatementAST *returnStmt = new ReturnStatementAST(returnValue);
  FunctionBlockAST mainBlock = FunctionBlockAST(&parsedBlock, *returnStmt);
  FunctionDeclarationAST *main = new FunctionDeclarationAST(type, name, args, mainBlock);

  main->createIR(*this, needPrintIR);
}

void Codegen::pp(BlockExprAST *block)
{
  block->pp();
}

Codegen::Codegen()
{
  InitializeNativeTarget();
  InitializeNativeTargetAsmPrinter();
  InitializeNativeTargetAsmParser();
  TheJIT = ExitOnErr(SimpleJIT::Create());
  initializeForJIT();
}

void Codegen::initializePassManagers()
{
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

void Codegen::initializeForJIT()
{
  // Open a new context and module.
  TheContext = std::make_unique<LLVMContext>();
  TheModule = std::make_unique<Module>("SimpleJIT", *TheContext);
  TheModule->setDataLayout(TheJIT->getDataLayout());
  addRuntime();

  // Create a new builder for the module.
  Builder = std::make_unique<IRBuilder<>>(*TheContext);
  initializePassManagers();
}

void Codegen::runCode(std::string inputFileName)
{
  std::cout << "Executing " << (inputFileName.empty() ? "from stdin" : inputFileName) << "\n";
  auto RT = TheJIT->getMainJITDylib().createResourceTracker();
  auto TSM = ThreadSafeModule(std::move(TheModule), std::move(TheContext));
  ExitOnErr(TheJIT->addModule(std::move(TSM), RT));
  initializeForJIT();

  auto ExprSymbol = ExitOnErr(TheJIT->lookup(MainFunctionName));
  // Get the symbol's address and cast it to the right function pointer type and call it as a native function.
  int (*FP)() = ExprSymbol.getAddress().toPtr<int (*)()>();
  int result = FP();

  std::cout << std::endl << "Exiting..." << std::endl;
  ExitOnErr(RT->remove());
  return;
}

void Codegen::writeObjFile(BlockExprAST &mainBlock, std::string optOutputFile)
{
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
    errs() << "Target lookup failed: " << Error;
    return;
  }

  auto CPU = "generic";
  auto Features = "";

  TargetOptions opt;
  auto TheTargetMachine = Target->createTargetMachine(
      TargetTriple, CPU, Features, opt, Reloc::PIC_);

  TheModule->setDataLayout(TheTargetMachine->createDataLayout());
  addRuntime();
  initializePassManagers();
  generateCode(mainBlock);

  auto Filename = optOutputFile.empty() ? "output.o" : optOutputFile;
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

void Codegen::optimize(llvm::Function *TheFunction)
{
  TheFPM->run(*TheFunction, *TheFAM);
}

/* Returns an LLVM type based on the identifier */
llvm::Type *Codegen::stringTypeToLLVM(const IdentifierExprAST &type)
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

bool Codegen::isTypeConversionPossible(llvm::Type *a, llvm::Type *b)
{
  llvm::Type *intType = Type::getInt32Ty(*TheContext);
  llvm::Type *doubleType = Type::getDoubleTy(*TheContext);
  return (a == intType && b == doubleType || a == doubleType && b == intType);
}

bool Codegen::typeCheck(BlockExprAST &mainBlock)
{
  // construct the name => type table for the main block
  NameTable *Names = new NameTable();
  NameTypesByBlock.push_back(Names);
  bool result = mainBlock.typeCheck(*this);
  return result;
}

std::string Codegen::print(llvm::Type *type)
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
void Codegen::addRuntime()
{
  /* MATH */
  TheModule->getOrInsertFunction(
      "fabs",
      FunctionType::get(
          Type::getDoubleTy(*TheContext),
          {Type::getDoubleTy(*TheContext)},
          false));
  TheModule->getOrInsertFunction(
      "pow",
      FunctionType::get(
          Type::getDoubleTy(*TheContext),
          {Type::getDoubleTy(*TheContext), Type::getDoubleTy(*TheContext)},
          false));
  TheModule->getOrInsertFunction(
      "sqrt",
      FunctionType::get(
          Type::getDoubleTy(*TheContext),
          {Type::getDoubleTy(*TheContext)},
          false));
  TheModule->getOrInsertFunction(
      "sin",
      FunctionType::get(
          Type::getDoubleTy(*TheContext),
          {Type::getDoubleTy(*TheContext)},
          false));
  TheModule->getOrInsertFunction(
      "cos",
      FunctionType::get(
          Type::getDoubleTy(*TheContext),
          {Type::getDoubleTy(*TheContext)},
          false));
  TheModule->getOrInsertFunction(
      "pi",
      FunctionType::get(
          Type::getDoubleTy(*TheContext),
          {},
          false));
  /* IO */
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
  TheModule->getOrInsertFunction(
      "readi",
      FunctionType::get(
        Type::getInt32Ty(*TheContext),
        {},
        false /* variadic func */
      ));
  TheModule->getOrInsertFunction(
      "readd",
      FunctionType::get(
        Type::getDoubleTy(*TheContext),
        {},
        false /* variadic func */
      ));
  TheModule->getOrInsertFunction(
      "readline",
      FunctionType::get(
        PointerType::getUnqual(Type::getInt8Ty(*TheContext)),
        {},
        true /* variadic func */
      ));
}
