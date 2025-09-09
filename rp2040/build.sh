#/bin/bash

cd ../rp2040
mkdir -p build
cd build
cmake ..
make
