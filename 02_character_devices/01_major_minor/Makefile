# Kbuild Makefile for major_minor_module
MODULE_NAME := major_minor_module
obj-m := $(MODULE_NAME).o

KDIR ?= /lib/modules/$(shell uname -r)/build
PWD  := $(shell pwd)

.PHONY: all clean load unload reload check_proc logs help

all:
	@echo "===> Compiling $(MODULE_NAME)..."
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	@echo "===> Cleaning artifacts..."
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	@rm -rf *.o *.ko *.mod *.mod.c .*.cmd .tmp_versions modules.order Module.symvers

load: all
	@echo "===> Loading $(MODULE_NAME)..."
	sudo insmod $(MODULE_NAME).ko
	@echo "===> Module loaded! Kernel logs:"
	@sudo dmesg | tail -n 12

check_proc:
	@echo "=========================================================="
	@echo "Inspecting /proc/devices for registered character device:"
	@echo "=========================================================="
	@cat /proc/devices | grep -E "Character|custom_char_dev" || echo "Device not found in /proc/devices."

unload:
	@echo "===> Unloading $(MODULE_NAME)..."
	sudo rmmod $(MODULE_NAME)
	@echo "===> Module removed! Kernel logs:"
	@sudo dmesg | tail -n 5

reload: unload load

logs:
	@sudo dmesg --color=always -T | grep -E "$(MODULE_NAME)" | tail -n 25

help:
	@echo "Available targets:"
	@echo "  make            - Build kernel module"
	@echo "  make clean      - Clean build files"
	@echo "  make load       - Insert module and allocate major/minor numbers"
	@echo "  make check_proc - View registered Major number in /proc/devices"
	@echo "  make unload     - Remove module and release numbers"
	@echo "  make logs       - View dmesg output"
