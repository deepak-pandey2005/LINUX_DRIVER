# Kbuild Makefile for multi-file kernel driver
MODULE_NAME := multi_file_driver

obj-m := $(MODULE_NAME).o
$(MODULE_NAME)-objs := multi_main.o sensor_math.o

KDIR ?= /lib/modules/$(shell uname -r)/build
PWD  := $(shell pwd)

.PHONY: all clean load unload reload logs help

all:
	@echo "===> Compiling multi-file module $(MODULE_NAME)..."
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	@echo "===> Cleaning artifacts..."
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	@rm -rf *.o *.ko *.mod *.mod.c .*.cmd .tmp_versions modules.order Module.symvers

load: all
	@echo "===> Loading $(MODULE_NAME)..."
	sudo insmod $(MODULE_NAME).ko
	@sudo dmesg | tail -n 8

unload:
	@echo "===> Unloading $(MODULE_NAME)..."
	sudo rmmod $(MODULE_NAME)
	@sudo dmesg | tail -n 5

reload: unload load

logs:
	@sudo dmesg --color=always -T | grep -E "$(MODULE_NAME)" | tail -n 20

help:
	@echo "Available targets:"
	@echo "  make        - Build multi-file module"
	@echo "  make clean  - Clean build artifacts"
	@echo "  make load   - Insert module into kernel"
	@echo "  make unload - Remove module from kernel"
