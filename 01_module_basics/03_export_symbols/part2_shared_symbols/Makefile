# Kbuild Makefile for symbol_producer and symbol_consumer

obj-m := symbol_producer.o symbol_consumer.o

KDIR ?= /lib/modules/$(shell uname -r)/build
PWD  := $(shell pwd)

.PHONY: all clean load_producer load_consumer unload_consumer unload_producer unload_all test_dependency_fail test_rmmod_fail check_dependency logs help

all:
	@echo "===> Compiling symbol_producer and symbol_consumer..."
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	@echo "===> Cleaning artifacts..."
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	@rm -rf *.o *.ko *.mod *.mod.c .*.cmd .tmp_versions modules.order Module.symvers

# Demonstrate failure: Loading consumer BEFORE producer
test_dependency_fail: all
	@echo "=========================================================="
	@echo "Attempting to load consumer WITHOUT producer loaded first:"
	@echo "=========================================================="
	-sudo insmod symbol_consumer.ko
	@echo "Check dmesg for 'Unknown symbol' error:"
	@sudo dmesg | tail -n 6

load_producer: all
	@echo "===> Loading symbol_producer into kernel..."
	sudo insmod symbol_producer.ko
	@sudo dmesg | tail -n 8

load_consumer: all
	@echo "===> Loading symbol_consumer into kernel..."
	sudo insmod symbol_consumer.ko
	@sudo dmesg | tail -n 8

check_dependency:
	@echo "=========================================================="
	@echo "Inspecting module dependency graph via lsmod:"
	@echo "=========================================================="
	@lsmod | grep -E "symbol_producer|symbol_consumer"
	@echo ""
	@echo "Checking exported symbols in /proc/kallsyms:"
	@sudo grep -E "compute_device_checksum|global_driver_event_count" /proc/kallsyms

# Demonstrate failure: Unloading producer while consumer is still using it
test_rmmod_fail:
	@echo "=========================================================="
	@echo "Attempting to unload producer WHILE consumer is still active:"
	@echo "=========================================================="
	-sudo rmmod symbol_producer
	@echo "^ Notice: The kernel protected memory safety by rejecting rmmod!"

unload_consumer:
	@echo "===> Unloading symbol_consumer..."
	sudo rmmod symbol_consumer
	@sudo dmesg | tail -n 5

unload_producer:
	@echo "===> Unloading symbol_producer..."
	sudo rmmod symbol_producer
	@sudo dmesg | tail -n 5

unload_all:
	@echo "===> Unloading in correct reverse dependency order..."
	-sudo rmmod symbol_consumer 2>/dev/null || true
	-sudo rmmod symbol_producer 2>/dev/null || true
	@echo "Done."

logs:
	@sudo dmesg --color=always -T | grep -E "symbol_producer|symbol_consumer" | tail -n 25

help:
	@echo "Available targets:"
	@echo "  make                     - Compile both modules"
	@echo "  make test_dependency_fail- Demonstrate failure when loading consumer first"
	@echo "  make load_producer       - Load producer module"
	@echo "  make load_consumer       - Load consumer module"
	@echo "  make check_dependency    - View module reference count and /proc/kallsyms"
	@echo "  make test_rmmod_fail     - Demonstrate kernel preventing unsafe unload"
	@echo "  make unload_all          - Safely unload all modules"
