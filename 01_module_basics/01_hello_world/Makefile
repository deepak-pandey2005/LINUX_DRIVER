# Kbuild Makefile for hello_module
# Out-of-tree kernel module build configuration

MODULE_NAME := hello_module
obj-m := $(MODULE_NAME).o

# Path to running kernel build tree
KDIR ?= /lib/modules/$(shell uname -r)/build
PWD  := $(shell pwd)

.PHONY: all clean load unload reload status logs help

all:
	@echo "===> Compiling $(MODULE_NAME) against kernel headers in $(KDIR)..."
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	@echo "===> Cleaning build artifacts..."
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	@rm -rf *.o *.ko *.mod *.mod.c .*.cmd .tmp_versions modules.order Module.symvers

load: all
	@echo "===> Inserting module into kernel..."
	sudo insmod $(MODULE_NAME).ko
	@echo "===> Module loaded! Check dmesg:"
	@sudo dmesg | tail -n 10

unload:
	@echo "===> Removing module from kernel..."
	sudo rmmod $(MODULE_NAME)
	@echo "===> Module removed! Check dmesg:"
	@sudo dmesg | tail -n 10

reload: unload load

status:
	@echo "--- Module Info ---"
	@modinfo $(MODULE_NAME).ko 2>/dev/null || echo "Module file not found. Run 'make' first."
	@echo ""
	@echo "--- Loaded Status (lsmod) ---"
	@lsmod | grep $(MODULE_NAME) || echo "Module $(MODULE_NAME) is NOT currently loaded."

logs:
	@sudo dmesg --color=always -T | grep -E "hello_module" | tail -n 25

help:
	@echo "Available targets:"
	@echo "  make         - Compile the kernel module"
	@echo "  make clean   - Remove all generated build artifacts"
	@echo "  make load    - Insert module into the running kernel (requires sudo)"
	@echo "  make unload  - Remove module from the running kernel (requires sudo)"
	@echo "  make reload  - Unload then load the module"
	@echo "  make status  - Display modinfo and check if currently loaded"
	@echo "  make logs    - Show kernel dmesg output for this module"
