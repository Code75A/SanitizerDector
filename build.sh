#!/bin/bash -e

# This script is to build the dynamic_analyzer used by ubsmith

# building dynamic_analyzer
export CC=clang-14
export CXX=clang++-14
rm -rf build/
mkdir build/
cd build/
cmake -DLLVM_DIR=/usr/lib/llvm-14/lib/cmake/llvm -DClang_DIR=/usr/lib/llvm-14/lib/cmake/clang ..
make
