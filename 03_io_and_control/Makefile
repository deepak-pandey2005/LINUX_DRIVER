# Makefile for Phase 3: Control Interfaces & Virtual Filesystems

SUBDIRS := 01_ioctl 02_procfs 03_sysfs

.PHONY: all clean load_ioctl test_ioctl unload_ioctl load_proc test_proc unload_proc load_sysfs test_sysfs unload_sysfs help $(SUBDIRS)

all: $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@ all

clean:
	@for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir clean; \
	done

# Lab 08 shortcuts (IOCTL Subsystem & Custom Commands)
load_ioctl:
	$(MAKE) -C 01_ioctl load

test_ioctl:
	$(MAKE) -C 01_ioctl test

unload_ioctl:
	$(MAKE) -C 01_ioctl unload

# Lab 09 shortcuts (Procfs Diagnostic Interface & seq_file)
load_proc:
	$(MAKE) -C 02_procfs load

test_proc:
	$(MAKE) -C 02_procfs test

unload_proc:
	$(MAKE) -C 02_procfs unload

# Lab 10 shortcuts (Sysfs Subsystem & kobjects)
load_sysfs:
	$(MAKE) -C 03_sysfs load

test_sysfs:
	$(MAKE) -C 03_sysfs test

unload_sysfs:
	$(MAKE) -C 03_sysfs unload

help:
	@echo "Phase 03: Control Interfaces & Virtual Filesystems"
	@echo "Available targets:"
	@echo "  make              - Build all completed submodules in Phase 3"
	@echo "  make clean        - Clean build artifacts"
	@echo "  make load_ioctl   - Load Chapter 08 (01_ioctl)"
	@echo "  make test_ioctl   - Run user-space IOCTL test suite on Chapter 08"
	@echo "  make unload_ioctl - Unload Chapter 08"
	@echo "  make load_proc    - Load Chapter 09 (02_procfs)"
	@echo "  make test_proc    - Run procfs test suite on Chapter 09"
	@echo "  make unload_proc  - Unload Chapter 09"
	@echo "  make load_sysfs   - Load Chapter 10 (03_sysfs)"
	@echo "  make test_sysfs   - Run sysfs test suite on Chapter 10"
	@echo "  make unload_sysfs - Unload Chapter 10"
