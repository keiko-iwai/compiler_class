#include <iostream>
extern int yyparse();

int main(int argc, char **argv)
{
    yyparse();
    std::cout << "Successfully parsed programBlock" << std::endl;
    return 0;
}
