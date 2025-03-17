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