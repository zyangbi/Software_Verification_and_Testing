#!/bin/bash
LLVM_TRANSFORMS_DIR="$HOME/llvm-project/llvm/lib/Transforms"
ASSIGNMENT="Assignment2"
ASSIGNMENT_DIR="$LLVM_TRANSFORMS_DIR/$ASSIGNMENT"

echo "add_subdirectory($ASSIGNMENT)" >> $LLVM_TRANSFORMS_DIR/CMakeLists.txt