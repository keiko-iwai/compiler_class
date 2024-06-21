#!/bin/sh
echo "...................................."
echo Compile strings.t to object file:
echo "...................................."
./compiler ./tests/basic_string.t
echo "...................................."
echo Compile object file with runtime:
echo "...................................."
clang++ runtime.cpp basic_string.o -o string
echo "...................................."
echo Run program:
echo "...................................."
./main
echo ""
