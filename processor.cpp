#include "exprAST.h"
#include "processor.h"
#include "parser.hpp"

void CodeGenContext::pp(BlockExprAST *block) {
    block->pp();
}
