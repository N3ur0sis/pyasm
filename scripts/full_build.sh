#!/bin/bash

echo "Building the project from scratch..."

./scripts/clean.sh
./scripts/build.sh
./scripts/run.sh "$1"