# Makefile for Phase 6: Device Model & Hardware Buses
SUBDIRS := 01_platform_driver 02_device_tree

.PHONY: all clean load_platform test_platform unload_platform load_dt test_dt unload_dt help $(SUBDIRS)

all: $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@ all

clean:
	@for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir clean; \
	done

# Lab 16 shortcuts (Platform Driver)
load_platform:
	$(MAKE) -C 01_platform_driver load

test_platform:
	$(MAKE) -C 01_platform_driver test

unload_platform:
	$(MAKE) -C 01_platform_driver unload

# Lab 17 shortcuts (Device Tree)
load_dt:
	$(MAKE) -C 02_device_tree load

test_dt:
	$(MAKE) -C 02_device_tree test

unload_dt:
	$(MAKE) -C 02_device_tree unload

help:
	@echo "Phase 06: Device Model & Hardware Buses"
	@echo "Available targets:"
	@echo "  make              - Build all completed submodules in Phase 6"
	@echo "  make clean        - Clean build artifacts"
	@echo "  make load_platform- Load Chapter 16 (01_platform_driver)"
	@echo "  make test_platform- Run platform driver test suite"
	@echo "  make unload_platform - Unload Chapter 16"
	@echo "  make load_dt      - Load Chapter 17 (02_device_tree)"
	@echo "  make test_dt      - Run Device Tree test suite"
	@echo "  make unload_dt    - Unload Chapter 17"
