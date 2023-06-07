#!/bin/bash
LLVM_TRANSFORMS_DIR="$HOME/llvm-project/llvm/lib/Transforms"
ASSIGNMENT_DIR="$LLVM_TRANSFORMS_DIR/Assignment1"

echo "add_subdirectory(Assignment1)" >> $LLVM_TRANSFORMS_DIR/CMakeLists.txt
if [ -d "$ASSIGNMENT_DIR" ]; then
    rm -rf "$ASSIGNMENT_DIR"
fi
mkdir "$ASSIGNMENT_DIR"
ln -s ~/Assignment1/Assignment1.cpp $ASSIGNMENT_DIR/Assignment1.cpp
ln -s ~/Assignment1/CMakeLists.txt $ASSIGNMENT_DIR/CMakeLists.txt

