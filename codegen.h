
#include "SimpleJIT.h"

#include <map>
#include <stack>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
/* compile to object file: */
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/TargetParser/Host.h"
/* -compile to object file: */
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/StandardInstrumentations.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Transforms/InstCombine/InstCombine.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Scalar/GVN.h"
#include "llvm/Transforms/Scalar/Reassociate.h"
#include "llvm/Transforms/Scalar/SimplifyCFG.h"

using namespace llvm;
using namespace llvm::orc;

class BlockExprAST;
class FunctionDeclarationAST;

class CodegenBlock
{
public:
  BasicBlock *block;
  std::map<std::string, AllocaInst *> locals;
};

typedef std::map<std::string, llvm::Type *> NameTable;
typedef struct {
  int refCount;
  int elemSize;
  int length;
  int maxLength;
  void *buf;
} Array;

class Codegen
{
  std::string MainFunctionName = std::string("main");
  int ConstObjCount = 0;

  /* LLVM modules and JIT module */
  std::unique_ptr<SimpleJIT> TheJIT;
  std::unique_ptr<FunctionPassManager> TheFPM;
  std::unique_ptr<LoopAnalysisManager> TheLAM;
  std::unique_ptr<FunctionAnalysisManager> TheFAM;
  std::unique_ptr<CGSCCAnalysisManager> TheCGAM;
  std::unique_ptr<ModuleAnalysisManager> TheMAM;
  std::unique_ptr<PassInstrumentationCallbacks> ThePIC;
  std::unique_ptr<StandardInstrumentations> TheSI;
  ExitOnError ExitOnErr;

public:
  /* LLVM resources */
  std::unique_ptr<LLVMContext> TheContext;
  std::unique_ptr<Module> TheModule;
  std::unique_ptr<IRBuilder<>> Builder;

  /* symbol tables */
  std::map<std::string, AllocaInst *> NamedValues;
  std::map<std::string, FunctionDeclarationAST *> *DefinedFunctions;
  std::vector<NameTable *> NameTypesByBlock;
  std::vector<Array *> AllocatedArrays;

  /* data structures for tracking the current block and function */
  std::stack<CodegenBlock *> GeneratingBlocks;
  std::stack<Function *> GeneratingFunctions;

  /* methods */
  Codegen();
  void initializePassManagers();
  void initializeForJIT();
  void addRuntime();

  void pp(BlockExprAST *block);
  bool typeCheck(BlockExprAST &block);
  void generateCode(BlockExprAST &block, bool withOptimization);
  void runCode();
  void writeObjFile(BlockExprAST &block);

  /* code generation functions */
  AllocaInst *createBlockAlloca(BasicBlock *BB, llvm::Type *type, const std::string &VarName);
  Value *createTypeCast(std::unique_ptr<IRBuilder<>> const &Builder, Value *value, llvm::Type *type);
  Value *createNonZeroCmp(std::unique_ptr<IRBuilder<>> const &Builder, Value *value);
  const std::string genStrConstantName();

  /* type helpers */
  llvm::Type *stringTypeToLLVM(const IdentifierExprAST &type);
  std::string print(llvm::Type *type);
  bool isTypeConversionPossible(llvm::Type *a, llvm::Type *b);

  std::map<std::string, AllocaInst *> &locals()
  {
    return GeneratingBlocks.top()->locals;
  }

  Function *currentFunction() { return GeneratingFunctions.top(); }
  void pushFunction(Function *fn) { GeneratingFunctions.push(fn); }
  void popFunction() { GeneratingFunctions.pop(); }

  BasicBlock *currentBlock() { return GeneratingBlocks.top()->block; }
  void pushBlock(BasicBlock *block)
  {
    GeneratingBlocks.push(new CodegenBlock());
    GeneratingBlocks.top()->block = block;
  }
  void popBlock()
  {
    CodegenBlock *top = GeneratingBlocks.top();
    GeneratingBlocks.pop();
    delete top;
  }
  void setFunctionList(std::map<std::string, FunctionDeclarationAST *> *DF)
  {
    DefinedFunctions = DF;
  }
};
