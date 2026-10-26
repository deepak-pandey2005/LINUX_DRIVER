# Makefile for Phase 2: Character Device Drivers

SUBDIRS := 01_major_minor 02_cdev_file_ops 03_udev_device_creation 04_ring_buffer

.PHONY: all clean load_num unload_num check_devices load_cdev test_cdev unload_cdev load_udev test_udev unload_udev load_ring test_ring unload_ring status logs help $(SUBDIRS)

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

# Lab 05 shortcuts (struct cdev & file_operations)
load_cdev:
	$(MAKE) -C 02_cdev_file_ops load

test_cdev:
	$(MAKE) -C 02_cdev_file_ops test

unload_cdev:
	$(MAKE) -C 02_cdev_file_ops unload

# Lab 06 shortcuts (class_create & udev dynamic node creation)
load_udev:
	$(MAKE) -C 03_udev_device_creation load

test_udev:
	$(MAKE) -C 03_udev_device_creation test

unload_udev:
	$(MAKE) -C 03_udev_device_creation unload

# Lab 07 shortcuts (safe memory transfer & circular ring buffer)
load_ring:
	$(MAKE) -C 04_ring_buffer load

test_ring:
	$(MAKE) -C 04_ring_buffer test

unload_ring:
	$(MAKE) -C 04_ring_buffer unload

help:
	@echo "Phase 02: Character Device Drivers"
	@echo "Available targets:"
	@echo "  make               - Build all submodules in Phase 2"
	@echo "  make clean         - Clean build artifacts"
	@echo "  make load_num      - Load Chapter 04 (01_major_minor)"
	@echo "  make check_devices - Check /proc/devices for registered Major number"
	@echo "  make unload_num    - Unload Chapter 04"
	@echo "  make load_cdev     - Load Chapter 05 (02_cdev_file_ops)"
	@echo "  make test_cdev     - Run user-space test app on Chapter 05"
	@echo "  make unload_cdev   - Unload Chapter 05"
	@echo "  make load_udev     - Load Chapter 06 (03_udev_device_creation)"
	@echo "  make test_udev     - Test unprivileged read/write on auto-created node"
	@echo "  make unload_udev   - Unload Chapter 06"
	@echo "  make load_ring     - Load Chapter 07 (04_ring_buffer)"
	@echo "  make test_ring     - Run wrap-around test suite on Chapter 07"
	@echo "  make unload_ring   - Unload Chapter 07"
