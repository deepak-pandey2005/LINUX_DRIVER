# Kbuild Makefile for ioctl_module and ioctl_test
MODULE_NAME := ioctl_module
obj-m := $(MODULE_NAME).o

KDIR ?= /lib/modules/$(shell uname -r)/build
PWD  := $(shell pwd)

DEV_PATH := /dev/ioctl_dev

.PHONY: all clean load test unload run_full_test logs help

all: ioctl_test
	@echo "===> Compiling $(MODULE_NAME)..."
	$(MAKE) -C $(KDIR) M=$(PWD) modules

ioctl_test: ioctl_test.c ioctl_commands.h
	@echo "===> Compiling ioctl_test..."
	gcc -Wall -O2 ioctl_test.c -o ioctl_test

clean:
	@echo "===> Cleaning artifacts..."
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	@rm -rf *.o *.ko *.mod *.mod.c .*.cmd .tmp_versions modules.order Module.symvers ioctl_test

load: all
	@echo "===> Loading $(MODULE_NAME)..."
	sudo insmod $(MODULE_NAME).ko
	@sleep 0.5
	@echo "===> Device status in /dev:"
	@ls -l $(DEV_PATH) 2>/dev/null || echo "Waiting for udev..."
	@echo "===> Kernel logs:"
	@sudo dmesg | tail -n 6

test: ioctl_test
	@if [ ! -e "$(DEV_PATH)" ]; then \
		echo "Device $(DEV_PATH) not found! Run 'make load' first."; exit 1; \
	fi
	@echo "===> Running IOCTL subsystem test suite..."
	./ioctl_test

unload:
	@echo "===> Unloading $(MODULE_NAME)..."
	sudo rmmod $(MODULE_NAME)
	@echo "===> Module unloaded cleanly. Kernel logs:"
	@sudo dmesg | tail -n 5

run_full_test: all load test unload
	@echo "===> Full automated test suite completed!"

logs:
	@sudo dmesg --color=always -T | grep -E "$(MODULE_NAME)" | tail -n 25

help:
	@echo "Available targets:"
	@echo "  make              - Compile kernel module and ioctl_test client"
	@echo "  make clean        - Clean build artifacts"
	@echo "  make load         - Load module and verify /dev/ioctl_dev"
	@echo "  make test         - Run IOCTL test suite"
	@echo "  make unload       - Unload module"
	@echo "  make run_full_test- Load, run tests, and unload automatically"
	@echo "  make logs         - View recent dmesg logs"
