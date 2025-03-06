#!/bin/bash
rm -rf build/
cmake -S . -B build/
cmake --build build/
./build/pyasm $1

# à commenter si les bonnes bibliothèques ne sont pas installées
dot -Tpdf ast.dot -o ast.pdf
