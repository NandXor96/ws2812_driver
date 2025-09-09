#!/bin/bash

rm -f mini_linux.tar.xz.
find ./buildroot/mini_linux/build -mindepth 1 -maxdepth 1 -type d -not -name configs -not -name overlay -exec rm -rf '{}' \;
tar cf - "buildroot/mini_linux/build" -P | pv -s $(du -sb "buildroot/mini_linux/build" | awk '{print $1}') | xz -3 -T0 >"mini_linux.tar.xz"
