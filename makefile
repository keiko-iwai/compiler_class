build: project

project: tokens.cpp parser.cpp
	clang++ -Xlinker --export-dynamic -g parser.cpp tokens.cpp processor.cpp codegen.cpp runtime.cpp main.cpp `llvm-config --cxxflags --ldflags --system-libs --libs core orcjit native`

tokens.cpp: tokens.l
	lex -o tokens.cpp tokens.l

parser.cpp:
	bison -d -o parser.cpp parser.y

clean:
	rm tokens.cpp parser.cpp parser.hpp