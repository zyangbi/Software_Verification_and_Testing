#!/bin/bash
cd ~/llvm-project/build/
core_count=$(nproc)
half_core_count=$((core_count / 2))
ninja -j"$half_core_count"



