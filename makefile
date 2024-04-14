build: project

project: tokens.cpp parser.cpp
	clang++ -g parser.cpp tokens.cpp processor.cpp main.cpp `llvm-config --cxxflags --ldflags --system-libs --libs core orcjit native`
#	clang++ -g parser.cpp tokens.cpp processor.cpp main.cpp

tokens.cpp: tokens.l
	lex -o tokens.cpp tokens.l

parser.cpp:
	bison -d -o parser.cpp parser.y

clean:
	rm tokens.cpp parser.cpp parser.hpp