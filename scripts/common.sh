ME_=$(basename "$0")

function log() {
    echo "${ME_}: $1"
}

function removeWithProgressAndSudo() {
    sudo rm -rfv "$1" | pv -l -s $(sudo du -a "$1" 2>/dev/null | wc -l) >/dev/null
}

function generateRandomString() {
    echo "$(tr -dc A-Za-z0-9 </dev/urandom | head -c $1)"
}

function generateRandomStringNoNum() {
    echo "$(tr -dc A-Za-z </dev/urandom | head -c $1)"
}

function updateDeviceFilesInContainer() {
    FILTER='^loop'

    #remove loop devs which have been removed on the host
    find /dev -mindepth 1 -maxdepth 1 -type b | cut -d/ -f3 | grep -E "$FILTER" | sort >/tmp/devs-created
    lsblk --raw -a --output "NAME" --noheadings | grep -E "$FILTER" | sort >/tmp/devs-available
    for ORPHAN in $(comm -23 /tmp/devs-created /tmp/devs-available); do
        sudo rm /dev/$ORPHAN
    done

    #detect added loop devs
    lsblk --raw -a --output "NAME,MAJ:MIN" --noheadings | grep -E "$FILTER" | while read LINE; do
        DEV=/dev/$(echo $LINE | cut -d' ' -f1)
        MAJMIN=$(echo $LINE | cut -d' ' -f2)
        MAJ=$(echo $MAJMIN | cut -d: -f1)
        MIN=$(echo $MAJMIN | cut -d: -f2)
        [ -b "$DEV" ] || sudo mknod "$DEV" b $MAJ $MIN
    done
}

function createLoopDevicesForImageFile() {
    IMAGE_FILE=$1

    updateDeviceFilesInContainer

    local -n DEVICES_=$2 # use nameref for indirection
    #DEVICES=()
    log "Creating loop devices for $IMAGE_FILE"
    loopDevice=$(sudo losetup --partscan --show --find "$IMAGE_FILE")
    DEVICES_+=("${loopDevice}")
    # drop the first line, as this is our LOOPDEV itself, but we only want the child partitions
    PARTITIONS=$(lsblk --raw --output "MAJ:MIN" --noheadings ${loopDevice} | tail -n +2)
    COUNTER=1
    for i in $PARTITIONS; do
        MAJ=$(echo $i | cut -d: -f1)
        MIN=$(echo $i | cut -d: -f2)
        if [ ! -e "${loopDevice}p${COUNTER}" ]; then
            sudo mknod ${loopDevice}p${COUNTER} b $MAJ $MIN
            DEVICES_+=("${loopDevice}p${COUNTER}")
        fi
        COUNTER=$((COUNTER + 1))
    done
}

function clearOrphanedLoopDevices() {

    updateDeviceFilesInContainer

    DEVICES=$(losetup -l | grep loop | grep .img | cut -d' ' -f1)
    for device in $DEVICES; do
        echo "Deleting orphaned loop device $device"
        sudo losetup -d "$device"
    done
}

function removeLoopDevicesForImageFile() {
    local -n DEVICES_=$1 # use nameref for indirection
    COUNTER=1
    for i in $DEVICES_; do
        sudo rm -f i
    done
    sudo losetup -d "${DEVICES_[0]}"
}

function mountPseudoFilesystems() {
    MOUNTPOINT=$1
    sudo mkdir -p "$MOUNTPOINT/dev" || true
    sudo mkdir -p "$MOUNTPOINT/dev/pts" || true
    sudo mkdir -p "$MOUNTPOINT/proc" || true
    sudo mkdir -p "$MOUNTPOINT/sys" || true

    sudo mount --bind /dev "$MOUNTPOINT/dev" || true
    sudo mount --bind /dev/pts "$MOUNTPOINT/dev/pts" || true
    sudo mount --bind /proc "$MOUNTPOINT/proc" || true
    sudo mount --bind /sys "$MOUNTPOINT/sys" || true
}

function unmountPseudoFilesystems() {
    MOUNTPOINT=$1
    sudo umount "$MOUNTPOINT/dev/pts" 2>/dev/null || true
    sudo umount "$MOUNTPOINT/dev" 2>/dev/null || true
    sudo umount "$MOUNTPOINT/proc" 2>/dev/null || true
    sudo umount "$MOUNTPOINT/sys" 2>/dev/null || true
}

function unmountBootAndESP() {
    MOUNTPOINT=$1

    loopDev1=$(mount | grep "$MOUNTPOINT/boot type" | cut -d' ' -f1)
    loopDev2=$(mount | grep "$MOUNTPOINT/boot/EFI type" | cut -d' ' -f1)

    sudo umount "$MOUNTPOINT/boot/EFI" 2>/dev/null || true
    sudo umount "$MOUNTPOINT/boot" 2>/dev/null || true

    sudo rm -f "$loopDev1" || true
    sudo rm -f "$loopDev2" || true

    sudo losetup -d "${loopDev1%p1*}" 2>/dev/null || true
    sudo losetup -d "${loopDev2%p1*}" 2>/dev/null || true
}

function unmount() {
    MOUNTPOINT=$1
    unmountBootAndESP "$MOUNTPOINT"
    unmountPseudoFilesystems "$MOUNTPOINT"
    sudo umount "$MOUNTPOINT" 2>/dev/null || true
}

function mountBootAndESP() {
    MOUNTPOINT=$1
    BOOT_IMAGE_FILE=$2
    ESP_IMAGE_FILE=$3
    unmountBootAndESP "$MOUNTPOINT"

    sudo mkdir -p "$MOUNTPOINT/boot" || true

    createLoopDevicesForImageFile "$BOOT_IMAGE_FILE" BOOT_IMAGE_DEVICES
    log "Allocated the following devices"
    declare -p BOOT_IMAGE_DEVICES

    if [ "${#BOOT_IMAGE_DEVICES[@]}" -eq "2" ] && [ -b "${BOOT_IMAGE_DEVICES[1]}" ]; then
        log "Checking and mounting filesystem in ${BOOT_IMAGE_DEVICES[1]}"
        sudo dosfsck -w -r -l -a -v -t "${BOOT_IMAGE_DEVICES[1]}" 1>/dev/null
        sudo mount "${BOOT_IMAGE_DEVICES[1]}" "$MOUNTPOINT/boot"
        sudo mkdir -p "$MOUNTPOINT/boot/EFI" || true
    else
        log "WARNING: allocating the bootfs loop devices went wrong"
    fi

    createLoopDevicesForImageFile "$ESP_IMAGE_FILE" ESP_IMAGE_DEVICES
    log "Allocated the following devices"
    declare -p ESP_IMAGE_DEVICES

    if [ "${#ESP_IMAGE_DEVICES[@]}" -eq "2" ] && [ -b "${ESP_IMAGE_DEVICES[1]}" ]; then
        log "Checking and mounting filesystem in ${ESP_IMAGE_DEVICES[1]}"
        sudo dosfsck -w -r -l -a -v -t "${ESP_IMAGE_DEVICES[1]}" 1>/dev/null
        sudo mount "${ESP_IMAGE_DEVICES[1]}" "$MOUNTPOINT/boot/EFI"
    else
        log "WARNING: allocating the ESPfs loop devices went wrong"
    fi

}

function mountBootAndESPAndRoot() {
    MOUNTPOINT=$1
    ROOT_IMAGE_FILE=$2
    BOOT_IMAGE_FILE=$3
    ESP_IMAGE_FILE=$4
    sudo mount "$ROOT_IMAGE_FILE" "$MOUNTPOINT" 2>/dev/null || true
    mountBootAndESP "$MOUNTPOINT" "$BOOT_IMAGE_FILE" "$ESP_IMAGE_FILE"
    mountPseudoFilesystems "$MOUNTPOINT"
}

function createDiskImageWithSingleVFATPartition() {
    IMAGE_FILE=$1
    IMAGE_SIZE_IN_MB=$2
    TYPE=$3
    LABEL="${4:-dos}"
    LABEL_ID="${5:-0xd5b425cb}"
    TEMP_MOUNTPOINT=$(generateRandomStringNoNum 12)

    read -r -d '' SFDISK_COMMAND <<EOF
label: $LABEL
label-id: $LABEL_ID
device: $TEMP_MOUNTPOINT
unit: sectors
sector-size: 512

${TEMP_MOUNTPOINT}1 : start= 2048, size= $((($IMAGE_SIZE_IN_MB * 1000000 - 2048) / 512)), type=$TYPE, bootable
EOF

    #echo -e "$SFDISK_COMMAND"

    log "Removing any old file named $IMAGE_FILE"
    rm -f "$IMAGE_FILE" || true
    log "Allocating $IMAGE_FILE with ${IMAGE_SIZE_IN_MB}MB"
    fallocate -l "${IMAGE_SIZE_IN_MB}M" "$IMAGE_FILE"

    log "Creating patitions in file $IMAGE_FILE"
    sudo echo "$SFDISK_COMMAND" | sfdisk "$IMAGE_FILE" 1>/dev/null

    createLoopDevicesForImageFile "$IMAGE_FILE" DEVICES
    log "Allocated the following devices"
    declare -p DEVICES

    if [ "${#DEVICES[@]}" -eq "2" ] && [ -b "${DEVICES[1]}" ]; then
        log "Creating filesystem in ${DEVICES[1]}"
        sudo mkfs.fat -F 32 "${DEVICES[1]}" 1>/dev/null
        sudo dosfsck -w -r -l -a -v -t "${DEVICES[1]}" 1>/dev/null
        mkdir -p "$TEMP_MOUNTPOINT"
        sudo mount "${DEVICES[1]}" "$TEMP_MOUNTPOINT/"

        log "Removing all things related to and including ${DEVICES[1]}"
        sudo umount "$TEMP_MOUNTPOINT/"
        sudo rm -rf "$TEMP_MOUNTPOINT"
    else
        log "WARNING: allocating the loop devices went wrong"
    fi

    removeLoopDevicesForImageFile DEVICES
}
