# Kbuild Makefile for cdev_module and user-space test_app
MODULE_NAME := cdev_module
obj-m := $(MODULE_NAME).o

KDIR ?= /lib/modules/$(shell uname -r)/build
PWD  := $(shell pwd)

DEV_NAME := cdev_buffer_dev
DEV_PATH := /dev/$(DEV_NAME)

.PHONY: all clean load create_node test remove_node unload run_full_test logs help

all: test_app
	@echo "===> Compiling $(MODULE_NAME)..."
	$(MAKE) -C $(KDIR) M=$(PWD) modules

test_app: test_app.c
	@echo "===> Compiling user-space test application..."
	gcc -Wall -O2 test_app.c -o test_app

clean:
	@echo "===> Cleaning artifacts..."
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	@rm -rf *.o *.ko *.mod *.mod.c .*.cmd .tmp_versions modules.order Module.symvers test_app

load: all
	@echo "===> Loading $(MODULE_NAME)..."
	sudo insmod $(MODULE_NAME).ko
	@echo "===> Module loaded! Kernel logs:"
	@sudo dmesg | tail -n 8

create_node:
	@MAJOR=$$(awk '$$2=="$(DEV_NAME)" {print $$1}' /proc/devices); \
	if [ -z "$$MAJOR" ]; then \
		echo "Error: $(DEV_NAME) not found in /proc/devices. Is module loaded?"; exit 1; \
	fi; \
	echo "===> Creating device node $(DEV_PATH) with Major $$MAJOR Minor 0..."; \
	sudo rm -f $(DEV_PATH); \
	sudo mknod $(DEV_PATH) c $$MAJOR 0; \
	sudo chmod 666 $(DEV_PATH); \
	ls -l $(DEV_PATH)

test: test_app
	@if [ ! -e "$(DEV_PATH)" ]; then \
		echo "Device node $(DEV_PATH) does not exist! Running 'make create_node' first..."; \
		$(MAKE) create_node; \
	fi
	@echo "===> Running user-space test application..."
	./test_app

remove_node:
	@echo "===> Removing $(DEV_PATH)..."
	-sudo rm -f $(DEV_PATH)

unload: remove_node
	@echo "===> Unloading $(MODULE_NAME)..."
	sudo rmmod $(MODULE_NAME)
	@sudo dmesg | tail -n 6

run_full_test: all load create_node test unload
	@echo "===> Full automated test complete!"

logs:
	@sudo dmesg --color=always -T | grep -E "$(MODULE_NAME)" | tail -n 25

help:
	@echo "Available targets:"
	@echo "  make              - Compile kernel module and user-space test app"
	@echo "  make clean        - Clean all build files"
	@echo "  make load         - Load kernel module"
	@echo "  make create_node  - Create /dev/cdev_buffer_dev with dynamic Major number"
	@echo "  make test         - Run user-space test application"
	@echo "  make unload       - Remove device node and unload kernel module"
	@echo "  make run_full_test- Automated end-to-end: load, create node, test, unload"
	@echo "  make logs         - View dmesg output"
