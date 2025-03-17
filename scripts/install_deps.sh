#!/bin/bash

echo "Checking and installing dependencies..."

# Check OS and install packages
if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    sudo apt update && sudo apt install -y cmake g++ graphviz
elif [[ "$OSTYPE" == "darwin"* ]]; then
    brew update
    brew install cmake graphviz
else
    echo "Unsupported OS. Install CMake, a C++ compiler, and Graphviz manually."
    exit 1
fi

echo "Dependencies installed successfully!"