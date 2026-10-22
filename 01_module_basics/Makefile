# Makefile for 01_module_basics phase

SUBDIRS := 01_hello_world 02_module_parameters

.PHONY: all clean load_hello unload_hello load_param load_param_custom test_param unload_param status logs help $(SUBDIRS)

all: $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@ all

clean:
	@for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir clean; \
	done

# Lab 01 shortcuts
load_hello:
	$(MAKE) -C 01_hello_world load

unload_hello:
	$(MAKE) -C 01_hello_world unload

# Lab 02 shortcuts
load_param:
	$(MAKE) -C 02_module_parameters load

load_param_custom:
	$(MAKE) -C 02_module_parameters load_custom

test_param:
	$(MAKE) -C 02_module_parameters test_sysfs

unload_param:
	$(MAKE) -C 02_module_parameters unload

status:
	@echo "--- Lab 01 ---"
	@$(MAKE) -C 01_hello_world status
	@echo ""
	@echo "--- Lab 02 ---"
	@$(MAKE) -C 02_module_parameters status

help:
	@echo "Phase 01: Module Basics"
	@echo "Available targets:"
	@echo "  make               - Build all submodules"
	@echo "  make clean         - Clean build artifacts"
	@echo "  make load_hello    - Load Lab 01 (01_hello_world)"
	@echo "  make unload_hello  - Unload Lab 01"
	@echo "  make load_param    - Load Lab 02 (02_module_parameters) with default values"
	@echo "  make load_param_custom - Load Lab 02 with custom command-line arguments"
	@echo "  make test_param    - Test dynamic sysfs reading, writing & validation"
	@echo "  make unload_param  - Unload Lab 02"
