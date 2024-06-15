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
  std::string optInputFile = "", optOutputFile = "";
  bool isOptEmitLLVM = false, isOptInteractive = false;

  auto cli = (
    opt_value("input file", optInputFile),
    option("-emit-llvm").set(isOptEmitLLVM).doc("emit llvm code"),
    option("-i").set(isOptInteractive).doc("run interactive"),
    option("-o") & value("output file", optOutputFile)
  );

  if (!parse(argc, argv, cli))
  {
    std::cout << make_man_page(cli, argv[0]);
    return 0;
  }

  if (optInputFile.empty() && isOptInteractive)
  {
    yyin = stdin;
  }
  else if (!(yyin = fopen(optInputFile.c_str(), "rt")))
  {
    std::cout << "Error: can not open file " << optInputFile << std::endl;
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
    context.generateCode(*programBlock, false, isOptEmitLLVM);
  else
  {
    std::cout << "Type errors found. Can not run code." << std::endl;
    return 1;
  }

  if (isOptEmitLLVM)
    return 0;

  if (isOptInteractive)
    context.runCode();
  else
    context.writeObjFile(*programBlock);

  free(buffer);
  return 0;
}
