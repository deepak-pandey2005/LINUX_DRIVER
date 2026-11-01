# Makefile for Phase 4: Concurrency, Locking & Memory
SUBDIRS := 01_locking 02_memory_mmap

.PHONY: all clean load_locking test_locking unload_locking load_mmap test_mmap unload_mmap help $(SUBDIRS)

all: $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@ all

clean:
	@for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir clean; \
	done

# Lab 11 shortcuts (Concurrency & Locking)
load_locking:
	$(MAKE) -C 01_locking load

test_locking:
	$(MAKE) -C 01_locking test

unload_locking:
	$(MAKE) -C 01_locking unload

# Lab 12 shortcuts (Zero-Copy mmap)
load_mmap:
	$(MAKE) -C 02_memory_mmap load

test_mmap:
	$(MAKE) -C 02_memory_mmap test

unload_mmap:
	$(MAKE) -C 02_memory_mmap unload

help:
	@echo "Phase 04: Concurrency, Synchronization & Memory Management"
	@echo "Available targets:"
	@echo "  make              - Build all completed submodules in Phase 4"
	@echo "  make clean        - Clean build artifacts"
	@echo "  make load_locking - Load Chapter 11 (01_locking)"
	@echo "  make test_locking - Run multi-threaded concurrency benchmark"
	@echo "  make unload_locking - Unload Chapter 11"
	@echo "  make load_mmap    - Load Chapter 12 (02_memory_mmap)"
	@echo "  make test_mmap    - Run zero-copy mmap test suite"
	@echo "  make unload_mmap  - Unload Chapter 12"
