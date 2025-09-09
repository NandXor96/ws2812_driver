SHELL := /bin/bash

name = usb_ws2812

obj-m += $(name).o

DEBUG_CFLAGS += -g -DDEBUG -O1
ccflags-y += ${DEBUG_CFLAGS}
#EXTRA_CFLAGS=-I$(PWD)/inc #deprecated

all:
	rm -rf ./build || true
	mkdir build
	make -C ../../linux M=$(PWD) modules
	echo "MODULE_INFO(intree, "Y");" >> $(name).mod.c
	rm $(name).ko $(name).o 
	make -C ../../linux M=$(PWD) modules
	shopt -s dotglob
	mv .*.cmd *.order *.ko *.mod *.o *.symvers *.mod.c ./build
	shopt -u dotglob
	sudo umount ../../rootfs || true
	mkdir -p ../../rootfs || true
	sudo mount ../../payloads/images/rootfs.ext2 ../../rootfs/
	sudo mkdir -p ../../rootfs/lib/modules/6.5.5/extra/
	sudo rm -f ../../rootfs/lib/modules/6.5.5/extra/$(name).ko || true
	sudo cp ./build/$(name).ko ../../rootfs/lib/modules/6.5.5/extra/
	sudo sed -i '/\/lib\/modules\/6.5.5\/extra\/usb_ws2812.ko:/d' ../../rootfs/lib/modules/6.5.5/modules.dep
	sudo sh -c 'echo "/lib/modules/6.5.5/extra/usb_ws2812.ko:" >> ../../rootfs/lib/modules/6.5.5/modules.dep'
	sudo umount ../../rootfs
	sudo rm -r ../../rootfs
clean:
	make -C ../../linux M=$(PWD) clean
	rm -rf ./build || true