# Kbuild Makefile for param_module
MODULE_NAME := param_module
obj-m := $(MODULE_NAME).o

KDIR ?= /lib/modules/$(shell uname -r)/build
PWD  := $(shell pwd)

SYSFS_DIR := /sys/module/$(MODULE_NAME)/parameters

.PHONY: all clean load load_custom unload reload test_sysfs status logs help

all:
	@echo "===> Compiling $(MODULE_NAME)..."
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	@echo "===> Cleaning build artifacts..."
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	@rm -rf *.o *.ko *.mod *.mod.c .*.cmd .tmp_versions modules.order Module.symvers

load: all
	@echo "===> Loading $(MODULE_NAME) with default parameters..."
	sudo insmod $(MODULE_NAME).ko
	@echo "===> Loaded! Kernel logs:"
	@sudo dmesg | tail -n 12

load_custom: all
	@echo "===> Loading $(MODULE_NAME) with custom parameters..."
	sudo insmod $(MODULE_NAME).ko sample_rate=5000 device_tag="SENSOR_ALPHA" verbose_logging=1 speed_limit=120 threshold_levels=5,15,25,35
	@echo "===> Loaded with custom params! Kernel logs:"
	@sudo dmesg | tail -n 15

unload:
	@echo "===> Unloading $(MODULE_NAME)..."
	sudo rmmod $(MODULE_NAME)
	@echo "===> Module removed! Kernel logs:"
	@sudo dmesg | tail -n 6

reload: unload load

status:
	@echo "--- Module Parameters Info ---"
	@modinfo -p $(MODULE_NAME).ko 2>/dev/null || echo "Run 'make' first."
	@echo ""
	@echo "--- Loaded Status ---"
	@lsmod | grep $(MODULE_NAME) || echo "Module is not currently loaded."

test_sysfs:
	@if [ ! -d "$(SYSFS_DIR)" ]; then \
		echo "Error: Module not loaded! Run 'make load' first."; exit 1; \
	fi
	@echo "=========================================================="
	@echo "1. Current sysfs parameters in $(SYSFS_DIR):"
	@echo "=========================================================="
	@for f in $(SYSFS_DIR)/*; do \
		echo "$$(basename $$f): $$(cat $$f 2>/dev/null || echo '<write-only or error>')"; \
	done
	@echo ""
	@echo "=========================================================="
	@echo "2. Dynamically updating speed_limit to 180 (valid [1-200])..."
	@echo "=========================================================="
	echo 180 | sudo tee $(SYSFS_DIR)/speed_limit
	@echo "Updated value: $$(cat $(SYSFS_DIR)/speed_limit)"
	@echo ""
	@echo "=========================================================="
	@echo "3. Attempting invalid speed_limit: 999 (should be rejected!)..."
	@echo "=========================================================="
	-echo 999 | sudo tee $(SYSFS_DIR)/speed_limit
	@echo ""
	@echo "=========================================================="
	@echo "4. Kernel log showing driver reaction:"
	@echo "=========================================================="
	@sudo dmesg | tail -n 10

logs:
	@sudo dmesg --color=always -T | grep -E "$(MODULE_NAME)" | tail -n 25

help:
	@echo "Available targets:"
	@echo "  make              - Compile kernel module"
	@echo "  make clean        - Clean build artifacts"
	@echo "  make load         - Load module with default arguments"
	@echo "  make load_custom  - Load module with custom test arguments"
	@echo "  make test_sysfs   - Test reading & dynamic runtime editing via sysfs"
	@echo "  make unload       - Remove module from kernel"
	@echo "  make status       - Inspect module parameters and load status"
	@echo "  make logs         - View dmesg output"
