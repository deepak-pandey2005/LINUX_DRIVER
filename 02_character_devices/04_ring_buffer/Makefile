# Kbuild Makefile for ring_buffer_module and test_client
MODULE_NAME := ring_buffer_module
obj-m := $(MODULE_NAME).o

KDIR ?= /lib/modules/$(shell uname -r)/build
PWD  := $(shell pwd)

DEV_PATH := /dev/ring_buffer_dev

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
	@echo "===> Device status in /dev:"
	@ls -l $(DEV_PATH) 2>/dev/null || echo "Waiting for udev..."
	@echo "===> Kernel logs:"
	@sudo dmesg | tail -n 6

test: test_client
	@if [ ! -e "$(DEV_PATH)" ]; then \
		echo "Device $(DEV_PATH) not found! Run 'make load' first."; exit 1; \
	fi
	@echo "===> Running ring buffer test suite..."
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
	@echo "  make              - Compile kernel module and user-space test client"
	@echo "  make clean        - Clean build artifacts"
	@echo "  make load         - Load module and verify /dev/ring_buffer_dev"
	@echo "  make test         - Run wrap-around, capacity, and fault-injection test suite"
	@echo "  make unload       - Unload module"
	@echo "  make run_full_test- Load, run tests, and unload automatically"
	@echo "  make logs         - View recent dmesg logs"
