#!/bin/bash

rm ~/.gdbinit

echo "add-auto-load-safe-path /home/vscode/workspace/linux" >~/.gdbinit
echo "source vmlinux-gdb.py" >>~/.gdbinit
echo "handle SIGUSR1 nostop noprint" >>~/.gdbinit
