#!/bin/bash
(
    set -e
    set -o pipefail

    # add the correct user perms
    sudo gpasswd -a root libvirt
    sudo gpasswd -a root kvm
    sudo chown root:kvm /dev/kvm

    # start the virtlogd daemon
    exec sudo virtlogd --daemon &
)