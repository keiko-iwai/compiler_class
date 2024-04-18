#include <iostream>
#include "exprAST.h"
#include "processor.h"

extern BlockExprAST *programBlock;
extern FunctionMap *definedFunctions;
extern int yyparse();

int main()
{
  CodeGenContext context;

  yyparse();

  if (!programBlock)
  {
    std::cout << "Invalid input. Nothing parsed" << std::endl;
    return 1;
  }
  context.pp(programBlock);
  context.setFunctionList(definedFunctions);

  if (context.typeCheck(*programBlock))
    context.generateCode(*programBlock);
  else
  {
    std::cout << "Type errors found. Can not run code." << std::endl;
    return 1;
  }

  context.runCode();
  return 0;
}
