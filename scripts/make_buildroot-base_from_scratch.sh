#!/bin/bash

CONFIG_PACKAGE_FILE=../payloads/buildroot_config/mini_linux.tar.xz
BUILDROOT_PACKAGE_FILE=../payloads/buildroot/buildroot-${BUILDROOT_NAME}.tar.gz
BUILDROOT_NAME=2023.08.1

#******************** CWD guard ********************
CWD=$(pwd)
SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &>/dev/null && pwd)

echo "**** Changing into script directory $SCRIPT_DIR"
cd "$SCRIPT_DIR"

. common.sh
#******************** CWD guard ********************

log "**** Remove old buildroot directory ****"
removeWithProgressAndSudo ../buildroot || true

log "**** Unpacking Buildroot ****"
cd $(dirname "$BUILDROOT_PACKAGE_FILE")
pv "$(basename "$BUILDROOT_PACKAGE_FILE")" | tar -xz
mv "buildroot-$BUILDROOT_NAME" "$SCRIPT_DIR/../buildroot"
cd "$SCRIPT_DIR"

log "**** Building Buildroot ****"
rm -rf ../buildroot/mini_linux || true
cd $(dirname "$CONFIG_PACKAGE_FILE")
tar -xvf "$(basename "$CONFIG_PACKAGE_FILE")"
mv buildroot/mini_linux "$SCRIPT_DIR/../buildroot/mini_linux"
cd "$SCRIPT_DIR/../buildroot/mini_linux/build"
rm -r images || true
make -j $(($(nproc) * 2))
rm "$SCRIPT_DIR/../payloads/images/rootfs.ext2"
cp images/rootfs.ext2 "$SCRIPT_DIR/../payloads/images/rootfs.ext2"

#******************** CWD guard ********************
log "**** Changing back into old working directory"
cd "$CWD"
#******************** CWD guard ********************
