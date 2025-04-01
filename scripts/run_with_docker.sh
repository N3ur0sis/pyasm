#!/bin/bash

if [ -z "$1" ]; then
    echo "Usage: ./scripts/run_with_docker.sh <input_file.mpy>"
    exit 1
fi

INPUT_FILE=$1
OUTPUT_DIR="$(pwd)"

# Step 1: Build the compiler if it doesn't exist
if [ ! -f "./build/bin/pyasm" ]; then
    ./scripts/native_build.sh
fi

# Step 2: Run the compiler to generate assembly
echo "Running pyasm compiler on $INPUT_FILE..."
./build/bin/pyasm "$INPUT_FILE"

# Step 3: Run the assembly in Docker
if [ -f "output.asm" ]; then
    echo "Running assembly in Docker..."
    
    # Run directly without building an image
    echo "Running assembly code in Docker container..."
    
    docker run --platform linux/amd64 --rm -v "$OUTPUT_DIR:/app" -w /app ubuntu:20.04 bash -c "
        apt-get update && 
        apt-get install -y nasm binutils &&
        nasm -f elf64 output.asm -o output.o && 
        ld -nostdlib output.o -o output && 
        echo 'Program output:' && 
        ./output
    "
else
    echo "No output.asm file generated."
    exit 1
fi