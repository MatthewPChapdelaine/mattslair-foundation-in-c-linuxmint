#!/bin/bash
set -e

echo "Installing dependencies..."
sudo apt update
sudo apt install -y build-essential cmake pkg-config libsdl2-dev libsdl2-image-dev

echo "Cleaning previous build..."
rm -f CMakeCache.txt
rm -rf CMakeFiles

echo "Building..."
cmake .
make

echo "Build complete! Run with: ./Altimit"
