#!/bin/bash

BUILDPATH=${1:-build}

rm -f CMakeCache.txt
rm -rf "$BUILDPATH"

echo "*************************** Stage 1 *************************************"
cmake -DCMAKE_BUILD_TYPE=Release -DUSE_GCC=1 -S . -B "$BUILDPATH"
cmake --build "$BUILDPATH" --parallel 4
echo "*************************** Stage 2 *************************************"
cmake -DCMAKE_BUILD_TYPE=Debug -DUSE_GCC=1 -DSAVETEMPS=--save-temps -S . -B "$BUILDPATH"
cmake --build "$BUILDPATH" --parallel 4
echo "*************************** Stage 3 *************************************"
cmake -DCMAKE_BUILD_TYPE=Debug -DUSE_GCC=1 -USAVETEMPS -S . -B "$BUILDPATH"
cmake --build "$BUILDPATH" --parallel 4
