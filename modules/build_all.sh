#!/bin/bash

#the presence of debug symobls can be verified with e.g. 'readelf -S block_dev1.ko  | grep debug'

for directory in */; do
    cd "$directory"
    make
    cd ..
done
