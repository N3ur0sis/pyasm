#!/bin/bash

# Check if argument provided
if [ -z "$1" ]; then
    echo "Usage: ./scripts/run.sh <input_file>"
    exit 1
fi

echo "Running pyasm with input: $1"

./build/bin/pyasm "$1"

# Convert ast.dot if exists
if [ -f "ast.dot" ]; then
    echo "Generating AST PDF..."
    dot -Tpdf ast.dot -o ast.pdf
    echo "AST visualization saved as ast.pdf"
else
    echo "No ast.dot file found. Skipping PDF generation."
fi

# Convert ast2.dot if exists
if [ -f "ast2.dot" ]; then
    echo "Generating AST PDF..."
    dot -Tpdf ast2.dot -o ast2.pdf
    echo "AST visualization saved as ast2.pdf"
else
    echo "No ast2.dot file found. Skipping PDF generation."
fi

# Execute the generated ASM file if it exists
echo -e "\nAssembling and executing output.asm..."
echo -e "\nProgram output:" && \
python3 "$1"
echo -e "\nExecution complete."
