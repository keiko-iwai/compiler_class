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
    return printf("%d\n", X);
  }

  int printd(double X)
  {
    return printf("%f\n", X);
  }

  int print(const char *fmt, ...) {
    int result = 0;
    va_list args;
    va_start(args, fmt);
    result = vprintf(fmt, args);
    va_end(args);
    return result;
  }

  int println(const char *fmt, ...) {
    int result = 0;
    va_list args;
    va_start(args, fmt);
    result = vprintf(fmt, args);
    va_end(args);
    putchar('\n');
    return result;
  }
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
