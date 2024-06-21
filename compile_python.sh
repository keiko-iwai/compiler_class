#!/bin/sh

PYTHONLIBVER=python$(python3 -c 'import sys; print(".".join(map(str, sys.version_info[:2])))')$(python3-config --abiflags)
gcc -Os $(python3-config --includes) $1 -o $2 $(python3-config --ldflags) -l$PYTHONLIBVER
