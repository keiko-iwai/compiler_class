#include <cstdio>
#include <cstdarg>
#include <cmath>
#include "exprAST.h"
#include "processor.h"

using namespace llvm;

extern "C"
{
  int printi(int X)
  {
    return fprintf(stderr, "%d\n", X);
  }

  int printd(double X)
  {
    return fprintf(stderr, "%f\n", X);
  }

  int printf(const char *fmt, ...);
  double sqrt(double X);
}

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
      "printf",
      FunctionType::get(
        Type::getInt32Ty(*TheContext),
        {Type::getInt8Ty(*TheContext)->getPointerTo()},
        true /* variadic func */
      ));
}
