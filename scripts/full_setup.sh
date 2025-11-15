#!/bin/bash

echo "Setting up the project from scratch..."

./scripts/clean.sh
./scripts/build.sh
./scripts/run.sh "$1"
