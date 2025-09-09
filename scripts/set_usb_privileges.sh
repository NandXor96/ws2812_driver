# Find usb device cafe:1234 and the corresponding bus and device id
lsusbstring=$(lsusb -d cafe:1234)
busid=$(echo $lsusbstring | awk '{print $2}' | sed 's/://')
deviceid=$(echo $lsusbstring | awk '{print $4}' | sed 's/://')
path=/dev/bus/usb/$busid/$deviceid

chown vscode:vscode $path
