#include <iostream>
#include <cstdlib>
#include "AST.h"
#include "codegen.h"
#include "error.h"
#include "external/clipp.h"
using namespace clipp;

extern BlockExprAST *programBlock;
extern FunctionMap *definedFunctions;

extern FILE *yyin;
extern int yyparse();

int main(int argc, char *argv[])
{
  // command line arguments
  std::string inputFile = "", outputFile = "";
  bool isEmitLLVM = false;

  auto cli = (
    value("input file", inputFile),
    option("-emit-llvm").set(isEmitLLVM).doc("emit llvm code"),
    option("-o") & value("output file", outputFile)
  );

  if (!parse(argc, argv, cli))
  {
    std::cout << make_man_page(cli, argv[0]);
    return 0;
  }

  if (inputFile.empty())
  {
    yyin = stdin;
  }
  else if (!(yyin = fopen(inputFile.c_str(), "rt")))
  {
    std::cout << "Error: can not open file " << inputFile << std::endl;
    return 1;
  }

  Codegen context;

  buffer = (char *)malloc(lMaxBuffer);
  while (getNextLine() == 0 && !parseError)
    yyparse();

  if (!programBlock || parseError)
  {
    // no message here as printError() function will print the parser errors
    return 2;
  }

  context.setFunctionList(definedFunctions);

  if (context.typeCheck(*programBlock))
    context.generateCode(*programBlock, false);
  else
  {
    std::cout << "Type errors found. Can not run code." << std::endl;
    return 1;
  }

  if (isEmitLLVM)
    return 0;

  context.runCode();
  context.writeObjFile(*programBlock);
  free(buffer);
  return 0;
}
