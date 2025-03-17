#!/bin/bash

echo "Setting up the project from scratch..."

./scripts/install_deps.sh
./scripts/clean.sh
./scripts/build.sh
./scripts/run.sh "$1"