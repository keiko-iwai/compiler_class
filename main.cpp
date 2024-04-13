#include <iostream>
#include "exprAST.h"
#include "processor.h"

extern BlockExprAST *programBlock;
extern int yyparse();

int main(int argc, char **argv)
{
    yyparse();
    CodeGenContext context;

    std::cout << "Successfully parsed programBlock" << std::endl;
    context.pp(programBlock);
    return 0;
}
