# Makefile for Phase 3: Control Interfaces & Virtual Filesystems

SUBDIRS := 01_ioctl

.PHONY: all clean load_ioctl test_ioctl unload_ioctl help $(SUBDIRS)

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

help:
	@echo "Phase 03: Control Interfaces & Virtual Filesystems"
	@echo "Available targets:"
	@echo "  make              - Build all completed submodules in Phase 3"
	@echo "  make clean        - Clean build artifacts"
	@echo "  make load_ioctl   - Load Chapter 08 (01_ioctl)"
	@echo "  make test_ioctl   - Run user-space IOCTL test suite on Chapter 08"
	@echo "  make unload_ioctl - Unload Chapter 08"
