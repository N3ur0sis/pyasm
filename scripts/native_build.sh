#!/bin/bash

echo "Building pyasm compiler natively..."

# Clean up previous build
rm -rf build/

# Build
mkdir -p build
cd build
cmake ..
cmake --build .
cd ..

echo "Native build complete!"