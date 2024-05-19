#include <iostream>
#include <cstdlib>
#include "AST.h"
#include "codegen.h"
#include "error.h"

extern BlockExprAST *programBlock;
extern FunctionMap *definedFunctions;
extern int yyparse();

int main()
{
  Codegen context;

  buffer = (char *)malloc(lMaxBuffer);
  while (getNextLine() == 0 && !parseError)
    yyparse();

  if (!programBlock || parseError)
  {
    // no message here as printError() function will print the parser errors
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
  free(buffer);
  return 0;
}
