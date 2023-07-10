#!/bin/bash
clang++ -O0 -g -S -emit-llvm -o "$1.ll" -c "$1.cpp"
opt -enable-new-pm=0 -load ~/llvm-project/build/lib/Assignment1.so -undeclvar < $1.ll > /dev/null
