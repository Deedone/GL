ARCH=arm64
CROSS_COMPILE=aarch64-linux-gnu-
obj-m := hello-module.o

KDIR := ~/src/linux
PWD := $(shell pwd)
default:
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) modules
clean:
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) clean