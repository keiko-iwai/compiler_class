#include <iostream>
#include "AST.h"
#include "codegen.h"

extern BlockExprAST *programBlock;
extern FunctionMap *definedFunctions;
extern int yyparse();

int main()
{
  Codegen context;

  yyparse();

  if (!programBlock)
  {
    std::cout << "Invalid input. Nothing parsed" << std::endl;
    return 1;
  }
  //context.pp(programBlock);
  context.setFunctionList(definedFunctions);

  if (context.typeCheck(*programBlock))
    context.generateCode(*programBlock, false);
  else
  {
    std::cout << "Type errors found. Can not run code." << std::endl;
    return 1;
  }

  context.runCode();
  context.writeObjFile(*programBlock);
  return 0;
}
