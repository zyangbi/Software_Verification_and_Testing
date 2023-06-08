#!/bin/bash
clang++ -O0 -g -S -emit-llvm -o HeapSort.ll -c HeapSort.cpp
opt -enable-new-pm=0 -load ~/llvm-project/build/lib/Assignment1.so -undeclvar < HeapSort.ll > /dev/null
