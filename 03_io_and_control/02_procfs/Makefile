# Kbuild Makefile for procfs_module and test_client
MODULE_NAME := procfs_module
obj-m := $(MODULE_NAME).o

KDIR ?= /lib/modules/$(shell uname -r)/build
PWD  := $(shell pwd)

PROC_DIR := /proc/driver_diagnostics

.PHONY: all clean load test unload run_full_test logs help

all: test_client
	@echo "===> Compiling $(MODULE_NAME)..."
	$(MAKE) -C $(KDIR) M=$(PWD) modules

test_client: test_client.c
	@echo "===> Compiling test_client..."
	gcc -Wall -O2 test_client.c -o test_client

clean:
	@echo "===> Cleaning artifacts..."
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	@rm -rf *.o *.ko *.mod *.mod.c .*.cmd .tmp_versions modules.order Module.symvers test_client

load: all
	@echo "===> Loading $(MODULE_NAME)..."
	sudo insmod $(MODULE_NAME).ko
	@sleep 0.5
	@echo "===> Checking $(PROC_DIR) contents:"
	@ls -la $(PROC_DIR) 2>/dev/null || echo "Waiting for proc entry..."
	@echo "===> Kernel logs:"
	@sudo dmesg | tail -n 6

test: test_client
	@if [ ! -d "$(PROC_DIR)" ]; then \
		echo "Proc directory $(PROC_DIR) not found! Run 'make load' first."; exit 1; \
	fi
	@echo "===> Running procfs test suite..."
	./test_client

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
	@echo "  make              - Compile kernel module and test_client"
	@echo "  make clean        - Clean build artifacts"
	@echo "  make load         - Load module and check /proc/driver_diagnostics"
	@echo "  make test         - Run procfs reading, writing, and validation tests"
	@echo "  make unload       - Unload module"
	@echo "  make run_full_test- Load, run tests, and unload automatically"
	@echo "  make logs         - View recent dmesg logs"
