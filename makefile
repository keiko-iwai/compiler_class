build: project

project: tokens.cpp parser.cpp
	clang++ -Xlinker --export-dynamic -g parser.cpp tokens.cpp codegen.cpp AST.cpp runtime.cpp main.cpp -o compiler `llvm-config --cxxflags --ldflags --system-libs --libs all`
## options -Xlinker --export-dynamic used in order to properly compile C function bindings
## use command objdump -T <executable> | grep <function_name> to see if there are specific symbols in the binary

tokens.cpp: tokens.l
	lex -o tokens.cpp tokens.l

parser.cpp:
	bison -d -o parser.cpp parser.y

clean:
	rm tokens.cpp parser.cpp parser.hpp