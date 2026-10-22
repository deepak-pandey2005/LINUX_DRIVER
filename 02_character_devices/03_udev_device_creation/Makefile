# Kbuild Makefile for udev_device_module and test_client
MODULE_NAME := udev_device_module
obj-m := $(MODULE_NAME).o

KDIR ?= /lib/modules/$(shell uname -r)/build
PWD  := $(shell pwd)

DEV_PATH := /dev/auto_char_dev
SYSFS_CLASS := /sys/class/auto_char_class

.PHONY: all clean load check_sysfs test unload run_full_test logs help

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
	@echo "===> Waiting 0.5s for udev to create node..."
	@sleep 0.5
	@echo "===> Node status in /dev (Notice: No mknod was run!):"
	@ls -l $(DEV_PATH) 2>/dev/null || echo "Node creation pending..."
	@echo "===> Kernel logs:"
	@sudo dmesg | tail -n 8

check_sysfs:
	@echo "=========================================================="
	@echo "1. Checking sysfs class hierarchy in $(SYSFS_CLASS):"
	@echo "=========================================================="
	@ls -la $(SYSFS_CLASS)/auto_char_dev 2>/dev/null || echo "Sysfs entry not found."
	@echo ""
	@echo "=========================================================="
	@echo "2. Device node in /dev:"
	@echo "=========================================================="
	@ls -la $(DEV_PATH) 2>/dev/null || echo "$(DEV_PATH) not found."

test: test_client
	@if [ ! -e "$(DEV_PATH)" ]; then \
		echo "Device $(DEV_PATH) not found! Run 'make load' first."; exit 1; \
	fi
	@echo "===> Running user-space client (without sudo!)..."
	./test_client

unload:
	@echo "===> Unloading $(MODULE_NAME)..."
	sudo rmmod $(MODULE_NAME)
	@echo "===> Confirming $(DEV_PATH) was removed automatically by udev:"
	@if [ ! -e "$(DEV_PATH)" ]; then \
		echo "SUCCESS: $(DEV_PATH) vanished cleanly!"; \
	else \
		echo "WARNING: $(DEV_PATH) still exists!"; \
	fi
	@sudo dmesg | tail -n 5

run_full_test: all load test unload
	@echo "===> Automated test passed!"

logs:
	@sudo dmesg --color=always -T | grep -E "$(MODULE_NAME)" | tail -n 25

help:
	@echo "Available targets:"
	@echo "  make              - Compile kernel module and user-space test client"
	@echo "  make clean        - Clean build artifacts"
	@echo "  make load         - Insert module (udev creates /dev/auto_char_dev automatically)"
	@echo "  make check_sysfs  - Inspect /sys/class and /dev node permissions"
	@echo "  make test         - Run user-space client"
	@echo "  make unload       - Remove module (udev cleans up /dev/auto_char_dev)"
	@echo "  make run_full_test- Full end-to-end automated verification"
