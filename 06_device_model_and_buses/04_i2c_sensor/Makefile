# Kbuild Makefile for i2c_sensor_module and test_client
MODULE_NAME := i2c_sensor_module
obj-m := $(MODULE_NAME).o

KDIR ?= /lib/modules/$(shell uname -r)/build
PWD  := $(shell pwd)

DEV_NODE := /dev/i2c_temp_sensor

.PHONY: all clean load test unload run_full_test logs help

all: test_client
	@echo "===> Compiling $(MODULE_NAME)..."
	$(MAKE) -C $(KDIR) M=$(PWD) modules

test_client: test_client.c i2c_sensor_ioctl.h
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
	@echo "===> Checking device node $(DEV_NODE):"
	@ls -la $(DEV_NODE) 2>/dev/null || echo "Waiting for udev node..."
	@echo "===> Kernel logs:"
	@sudo dmesg | tail -n 8

test: test_client
	@if [ ! -e "$(DEV_NODE)" ]; then \
		echo "Device node $(DEV_NODE) not found! Run 'make load' first."; exit 1; \
	fi
	@echo "===> Running I2C sensor verification test..."
	sudo chmod 666 $(DEV_NODE) 2>/dev/null || true
	./test_client

unload:
	@echo "===> Unloading $(MODULE_NAME)..."
	sudo rmmod $(MODULE_NAME)
	@echo "===> Module unloaded cleanly. Kernel logs:"
	@sudo dmesg | tail -n 5

run_full_test: all load test unload
	@echo "===> Full automated I2C sensor test completed!"

logs:
	@sudo dmesg --color=always -T | grep -E "$(MODULE_NAME)|lm75" | tail -n 25

help:
	@echo "Available targets:"
	@echo "  make              - Compile kernel module and test_client"
	@echo "  make clean        - Clean build artifacts"
	@echo "  make load         - Load module into kernel"
	@echo "  make test         - Run I2C sensor test suite"
	@echo "  make unload       - Unload module"
	@echo "  make run_full_test- Load, run tests, and unload automatically"
	@echo "  make logs         - View recent dmesg logs"

