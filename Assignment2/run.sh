#!/bin/bash
clang++ -O0 -g -S -emit-llvm -fno-discard-value-names -o Test/$1/$1.ll -c Test/$1/$1.cpp
opt -enable-new-pm=0 -load ~/llvm-project/build/lib/Assignment2.so -taintanalysis < Test/$1/$1.ll > /dev/null