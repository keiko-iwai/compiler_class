#include <iostream>
#include "exprAST.h"
#include "processor.h"

extern BlockExprAST *programBlock;
extern int yyparse();

int main()
{
    yyparse();
    CodeGenContext context;

    if (!programBlock) {
        std::cout << "Invalid input. Nothing parsed" << std::endl;
        return 1;
    }
    context.pp(programBlock);
    context.generateCode(*programBlock);
    context.runCode();
    return 0;
}
