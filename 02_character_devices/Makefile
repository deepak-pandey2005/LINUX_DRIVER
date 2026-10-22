# Makefile for Phase 2: Character Device Drivers

SUBDIRS := 01_major_minor

.PHONY: all clean load_num unload_num check_devices status logs help $(SUBDIRS)

all: $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@ all

clean:
	@for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir clean; \
	done

# Lab 04 shortcuts (Major & Minor numbers)
load_num:
	$(MAKE) -C 01_major_minor load

unload_num:
	$(MAKE) -C 01_major_minor unload

check_devices:
	$(MAKE) -C 01_major_minor check_proc

logs:
	$(MAKE) -C 01_major_minor logs

help:
	@echo "Phase 02: Character Device Drivers"
	@echo "Available targets:"
	@echo "  make               - Build all submodules in Phase 2"
	@echo "  make clean         - Clean build artifacts"
	@echo "  make load_num      - Load Chapter 04 (01_major_minor)"
	@echo "  make check_devices - Check /proc/devices for registered Major number"
	@echo "  make unload_num    - Unload Chapter 04"
	@echo "  make logs          - Show dmesg logs"
