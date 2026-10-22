# Top-level Makefile for Linux Driver Curriculum

SUBDIRS := 01_module_basics/01_hello_world \
           01_module_basics/02_module_parameters \
           01_module_basics/03_export_symbols \
           02_character_devices/01_major_minor \
           02_character_devices/02_cdev_file_ops

.PHONY: all clean help $(SUBDIRS)

all: $(SUBDIRS)

$(SUBDIRS):
	@echo "=========================================================="
	@echo "Building $@"
	@echo "=========================================================="
	$(MAKE) -C $@ all

clean:
	@for dir in $(SUBDIRS); do \
		echo "Cleaning $$dir..."; \
		$(MAKE) -C $$dir clean; \
	done

help:
	@echo "Linux Kernel Driver Development Repository"
	@echo "==========================================="
	@echo "Targets:"
	@echo "  make         - Build all modules"
	@echo "  make clean   - Clean all module build artifacts"
	@echo "  make help    - Show this help message"
	@echo ""
	@echo "To work with a specific lab:"
	@echo "  cd 01_module_basics/01_hello_world && make"
