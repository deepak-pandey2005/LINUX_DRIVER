# Makefile for Phase 5: Interrupts & Timers
SUBDIRS := 01_interrupts 02_workqueues 03_timers

.PHONY: all clean load_irq test_irq unload_irq load_wq test_wq unload_wq load_timer test_timer unload_timer help $(SUBDIRS)

all: $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@ all

clean:
	@for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir clean; \
	done

# Lab 13 shortcuts (Hardware & Shared Interrupts)
load_irq:
	$(MAKE) -C 01_interrupts load

test_irq:
	$(MAKE) -C 01_interrupts test

unload_irq:
	$(MAKE) -C 01_interrupts unload

# Lab 14 shortcuts (Deferred Execution & Workqueues)
load_wq:
	$(MAKE) -C 02_workqueues load

test_wq:
	$(MAKE) -C 02_workqueues test

unload_wq:
	$(MAKE) -C 02_workqueues unload

# Lab 15 shortcuts (Kernel Timers & Delays)
load_timer:
	$(MAKE) -C 03_timers load

test_timer:
	$(MAKE) -C 03_timers test

unload_timer:
	$(MAKE) -C 03_timers unload

help:
	@echo "Phase 05: Interrupts, Deferred Execution & Kernel Timers"
	@echo "Available targets:"
	@echo "  make              - Build all completed submodules in Phase 5"
	@echo "  make clean        - Clean build artifacts"
	@echo "  make load_irq     - Load Chapter 13 (01_interrupts)"
	@echo "  make test_irq     - Run interrupt test suite"
	@echo "  make unload_irq   - Unload Chapter 13"
	@echo "  make load_wq      - Load Chapter 14 (02_workqueues)"
	@echo "  make test_wq      - Run workqueue test suite"
	@echo "  make unload_wq    - Unload Chapter 14"
	@echo "  make load_timer   - Load Chapter 15 (03_timers)"
	@echo "  make test_timer   - Run kernel timers test suite"
	@echo "  make unload_timer - Unload Chapter 15"

