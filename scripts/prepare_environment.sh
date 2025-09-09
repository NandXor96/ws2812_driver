#!/bin/bash

KERNEL_NAME=6.5.5
KERNEL_PACKAGE_FILE=../payloads/kernel/linux-${KERNEL_NAME}.tar.xz
BUILDROOT_FS_FILE=../payloads/images/rootfs.ext2.xz
MOUNTPOINT=temp

#******************** CWD guard ********************
CWD=$(pwd)
SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &>/dev/null && pwd)

echo "**** Changing into script directory $SCRIPT_DIR"
cd "$SCRIPT_DIR"

. common.sh
#******************** CWD guard ********************

log "**** Precheck: checking for HOSTNAME environment variable ******"
if [ -z ${HOSTNAME+x} ]; then
    echo "HOSTNAME is unset, bailing out"
    exit 1
else
    echo "HOSTNAME is set to '$HOSTNAME'"
fi

if [ -z "$HOSTNAME" ]; then
    echo "HOSTNAME is set but empty, bailing out"
    exit 1
else
    echo "HOSTNAME is NOT empty"
fi

if pgrep -f "/usr/local/bin/qemu-system-x86_64" >/dev/null; then
    echo "qemu-system-x86_64 is running, please stop all instances before continuing! Bailing out"
    echo "Close the following PIDs $(pgrep -f "/usr/local/bin/qemu-system-x86_64")"
    exit 1
else
    echo "qemu-system-x86_64 is not running!"
fi

log "**** Cleanup: Unmounting boot, ESP, and root filesystems ******"
unmountBootAndESP "$MOUNTPOINT"
unmountPseudoFilesystems "$MOUNTPOINT"
clearOrphanedLoopDevices

log "**** Removing old build artifacts ****"
log "old linux directory"
removeWithProgressAndSudo ../linux || true

log "**** Unpacking Kernel ****"
cd $(dirname "$KERNEL_PACKAGE_FILE")
pv "$(basename "$KERNEL_PACKAGE_FILE")" | tar -xJ
mv "linux-$KERNEL_NAME" "$SCRIPT_DIR/../linux"
rm "$SCRIPT_DIR/../linux/.gitignore"
cd "$SCRIPT_DIR"

log "**** Configuring and compiling Kernel ****"
cd ../linux && yes "" | make defconfig kvm_guest.config && ./scripts/config -e DEBUG_INFO -e DEBUG_KERNEL -e DEBUG_INFO_DWARF4 -d DEBUG_INFO_BTF -e GDB_SCRIPTS -d CONFIG_DEBUG_INFO_REDUCED \
    -d CONFIG_RANDOMIZE_BASE -e CONFIG_FRAME_POINTER -e DEBUG_SECTION_MISMATCH -e DEBUG_OBJECTS -e DEBUG_OBJECTS_WORK -e DEBUG_VM -e HEADERS_INSTALL -e EFI -e EFI_STUB -e X86_SYSFB -e FB_SIMPLE -e FRAMEBUFFER_CONSOLE -e EFIVAR_FS -d EFI_VARS -e FB -e FB_MODE_HELPERS -e FB_VGA16 -e FB_UVESA \
    -e FB_VESA -e FB_EFI -d FB_NVIDIA -d FB_NVIDIA_BACKLIGHT && yes "" | make -j $(($(nproc) * 2)) && cd "$SCRIPT_DIR"
rm "$SCRIPT_DIR/../payloads/images/bzImage"
cp "$SCRIPT_DIR/../linux/arch/x86_64/boot/bzImage" "$SCRIPT_DIR/../payloads/images/bzImage"

log "**** Unpacking buildroot filesystem ****"
rm "$SCRIPT_DIR/../payloads/images/rootfs.ext2"
unxz -k "$BUILDROOT_FS_FILE"

log "**** Setting gdb settings ****"
echo "add-auto-load-safe-path /home/vscode/workspace/linux" >~/.gdbinit
echo "source vmlinux-gdb.py" >>~/.gdbinit
echo "handle SIGUSR1 nostop noprint" >>~/.gdbinit

log "**** Compiling Kernel modules ****"
cd ../modules
chmod u+x build_all.sh
./build_all.sh
cd "$SCRIPT_DIR"

log "**** Compiling Userland programs ****"
cd ../user_programs
chmod u+x compile.sh
./compile.sh
cd "$SCRIPT_DIR"

log "**** Compiling RP2040 Controller program ****"
../rp2040/build.sh
cd "$SCRIPT_DIR"

#******************** CWD guard ********************
log "**** Changing back into old working directory"
cd "$CWD"
#******************** CWD guard ********************
