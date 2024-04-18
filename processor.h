
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

class CodeGenBlock
{
public:
  BasicBlock *block;
  std::map<std::string, AllocaInst *> locals;
};

typedef std::map<std::string, llvm::Type *> NameTable;
class CodeGenContext
{
  std::string _mainFunctionName = std::string("main");
  Function *_mainFunction;

public:
  std::unique_ptr<LLVMContext> TheContext;
  std::unique_ptr<Module> TheModule;
  std::unique_ptr<IRBuilder<>> Builder;
  std::unique_ptr<SimpleJIT> TheJIT;
  std::unique_ptr<FunctionPassManager> TheFPM;
  std::unique_ptr<LoopAnalysisManager> TheLAM;
  std::unique_ptr<FunctionAnalysisManager> TheFAM;
  std::unique_ptr<CGSCCAnalysisManager> TheCGAM;
  std::unique_ptr<ModuleAnalysisManager> TheMAM;
  std::unique_ptr<PassInstrumentationCallbacks> ThePIC;
  std::unique_ptr<StandardInstrumentations> TheSI;
  ExitOnError ExitOnErr;

  std::map<std::string, AllocaInst *> NamedValues;
  std::stack<CodeGenBlock *> _blocks;
  std::map<std::string, FunctionDeclarationAST *> DefinedFunctions;
  std::vector<NameTable *> NamesByBlock;

  CodeGenContext();
  void InitializeModuleAndManagers();
  void AddRuntime();

  void pp(BlockExprAST *block);
  bool typeCheck(BlockExprAST &block);
  void generateCode(BlockExprAST &block);
  void runCode();
  AllocaInst *CreateBlockAlloca(BasicBlock *BB, Type *type, const std::string &VarName);
  Value *CreateTypeCast(std::unique_ptr<IRBuilder<>> const &Builder, Value *value, Type *type);

  Type *stringTypeToLLVM(const IdentifierExprAST &type);
  std::string print(Type *type);
  bool isTypeConversionPossible(Type *a, Type *b);

  std::map<std::string, AllocaInst *> &locals()
  {
    return _blocks.top()->locals;
  }
  BasicBlock *currentBlock() { return _blocks.top()->block; }
  void pushBlock(BasicBlock *block)
  {
    _blocks.push(new CodeGenBlock());
    _blocks.top()->block = block;
  }
  void popBlock()
  {
    CodeGenBlock *top = _blocks.top();
    _blocks.pop();
    delete top;
  }
  void setFunctionList(std::map<std::string, FunctionDeclarationAST *> DefinedFunctions)
  {
    DefinedFunctions = DefinedFunctions;
  }
};
