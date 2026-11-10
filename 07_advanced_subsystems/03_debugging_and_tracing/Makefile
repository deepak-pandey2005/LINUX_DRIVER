obj-m += debug_kprobe_module.o

KDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

EXTRA_CFLAGS += -Wall -O2

all: modules test_client

modules:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

test_client: test_client.c
	$(CC) $(EXTRA_CFLAGS) -o $@ $<

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f test_client

.PHONY: all modules clean
