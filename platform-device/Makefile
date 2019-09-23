ARCH = arm64
CROSS_COMPILE = aarch64-linux-gnu-
obj-m := platform-device.o


KDIR := /home/mpoturai/src/linux 
#KDIR := /home/mpoturai/src/build/tmp/work-shared/h3ulcb/kernel-source 
PWD := $(shell pwd) 
default:
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) modules
clean:
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) clean