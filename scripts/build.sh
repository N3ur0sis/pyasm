#!/bin/bash

echo "Building the project..."

mkdir -p build
cd build


cmake ..
cmake --build .

echo "Build complete!"