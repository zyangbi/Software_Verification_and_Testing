#!/bin/bash
clang++ -O0 -g -S -emit-llvm -fno-discard-value-names -o Conditional.ll -c Conditional.cpp
opt -enable-new-pm=0 -load ~/llvm-project/build/lib/Assignment2.so -taintanalysis < Conditional.ll > /dev/null