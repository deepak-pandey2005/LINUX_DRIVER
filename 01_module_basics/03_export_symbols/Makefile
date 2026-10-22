# Aggregator Makefile for Chapter 03

SUBDIRS := part1_multifile part2_shared_symbols

.PHONY: all clean help $(SUBDIRS)

all: $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@ all

clean:
	@for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir clean; \
	done

help:
	@echo "Chapter 03: Multi-File Modules & Exporting Symbols"
	@echo "Subdirectories:"
	@echo "  part1_multifile       - Compiling multiple .c files into a single .ko"
	@echo "  part2_shared_symbols  - Exporting and consuming symbols across distinct modules"
