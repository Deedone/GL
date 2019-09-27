ARCH = arm64
CROSS_COMPILE = aarch64-linux-gnu-
obj-m := chardev.o
chardev-objs := device.o char-device.o driver.o


KDIR := /home/mpoturai/src/linux 
#KDIR := /home/mpoturai/src/build/tmp/work-shared/h3ulcb/kernel-source 
PWD := $(shell pwd) 
default: userspace
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) modules
	sudo cp chardev.ko /home/mpoturai/rfs_board
	sudo cp send.o /home/mpoturai/rfs_board
	sudo cp get.o /home/mpoturai/rfs_board
	sudo cp ioctl.o /home/mpoturai/rfs_board
clean:
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) clean

userspace: send-data.c get-data.c
	$(CROSS_COMPILE)gcc send-data.c -o send.o
	$(CROSS_COMPILE)gcc send-ioctl.c -o ioctl.o
	$(CROSS_COMPILE)gcc get-data.c -o get.o

