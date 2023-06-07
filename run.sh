#!/bin/bash
clang++ -O0 -S -emit-llvm -o HelloWorld.ll -c HelloWorld.cpp
opt -enable-new-pm=0 -load ~/llvm-project/build/lib/Assignment1.so -undeclvar < HelloWorld.ll > /dev/null
