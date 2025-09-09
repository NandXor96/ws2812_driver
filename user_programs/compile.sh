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

echo "*************************** Stage 4 *************************************"
sudo umount ../rootfs || true
mkdir -p ../rootfs || true
sudo mount ../payloads/images/rootfs.ext2 ../rootfs/
sudo mkdir -p ../rootfs/root/user_programs/ || true
sudo rm -f ../rootfs/root/user_programs/* || true
sudo cp ./build/build/bin/debug/* ../rootfs/root/user_programs/
sudo cp ./build/build/lib/debug/*.so ../rootfs/usr/lib64/
sudo cp -r ./examples/* ../rootfs/root/user_programs/
sudo chmod u+x ../rootfs/root/user_programs/runexamples.sh
sudo umount ../rootfs
sudo rm -r ../rootfs
