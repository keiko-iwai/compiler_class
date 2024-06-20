#include <iostream>
#include <cstdlib>
#include <locale>
#include "AST.h"
#include "codegen.h"
#include "error.h"
#include "external/clipp.h"
using namespace clipp;

#if defined(WIN32) || defined(_WIN32)
#define PATH_SEPARATOR "/\\"
#else
#define PATH_SEPARATOR "/"
#endif

extern BlockExprAST *programBlock;
extern FunctionMap *definedFunctions;

extern FILE *yyin;
extern int yyparse();

int main(int argc, char *argv[])
{
  std::locale::global(std::locale("en_US.UTF-8"));
  // command line arguments
  std::string optInputFile = "", optOutputFile = "";
  bool isOptEmitLLVM = false, isOptInteractive = false;
  std::string objectFile, llvmFile;

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

  if (optInputFile.empty())
    yyin = stdin;
  else if (!(yyin = fopen(optInputFile.c_str(), "rt")))
  {
    std::cout << "Error: can not open file " << optInputFile << std::endl;
    return 1;
  }

  std::string inputFileName = optInputFile.substr(optInputFile.find_last_of("/\\") + 1);
  std::string::size_type const pointPos(inputFileName.find_last_of('.'));
  std::string baseFileName = inputFileName.substr(0, pointPos);
  if (baseFileName.empty())
    baseFileName = "output";

  objectFile = !optOutputFile.empty() ? optOutputFile
    : !baseFileName.empty() ? baseFileName + ".o"
    : "output";

  llvmFile = !optOutputFile.empty() ? optOutputFile : baseFileName + ".ll";

  // parse
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
    context.generateCode(*programBlock, false, isOptEmitLLVM, llvmFile);
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
    context.writeObjFile(*programBlock, objectFile);

  free(buffer);
  return 0;
}
