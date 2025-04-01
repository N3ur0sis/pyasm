#!/bin/bash

echo "Building the project..."

mkdir -p build
cd build

# Use your system compiler for building the Python-to-ASM converter
cmake ..
cmake --build .

echo "Build complete!"