#!/bin/bash
LLVM_TRANSFORMS_DIR="$HOME/llvm-project/llvm/lib/Transforms"
ASSIGNMENT="Assignment1"
ASSIGNMENT_DIR="$LLVM_TRANSFORMS_DIR/$ASSIGNMENT"

if [ -d "$ASSIGNMENT_DIR" ]; then
    rm -rf "$ASSIGNMENT_DIR"
fi
mkdir "$ASSIGNMENT_DIR"
ln -s $(pwd)/$ASSIGNMENT.cpp $ASSIGNMENT_DIR/$ASSIGNMENT.cpp
ln -s $(pwd)/CMakeLists.txt $ASSIGNMENT_DIR/CMakeLists.txt
