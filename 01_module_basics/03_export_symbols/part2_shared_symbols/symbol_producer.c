// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file symbol_producer.c
 * @brief Chapter 03 (Part 2): Kernel Symbol Producer Module.
 * @author Deepak Pandey
 *
 * Demonstrates exporting functions and variables into the global kernel
 * symbol table using `EXPORT_SYMBOL_GPL()`.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include "symbol_producer.h"

// Define the exported global variable
int global_driver_event_count = 100;
EXPORT_SYMBOL_GPL(global_driver_event_count);

// Define the exported function
u32 compute_device_checksum(const u8 *buffer, size_t len)
{
    u32 sum1 = 0xffff, sum2 = 0xffff;
    size_t i;

    if (!buffer || len == 0) {
        return 0;
    }

    for (i = 0; i < len; i++) {
        sum1 = (sum1 + buffer[i]) % 65535;
        sum2 = (sum2 + sum1) % 65535;
    }

    pr_info("compute_device_checksum called for len=%zu -> checksum=0x%08x\n",
            len, (sum2 << 16) | sum1);

    return (sum2 << 16) | sum1;
}
EXPORT_SYMBOL_GPL(compute_device_checksum);

static int __init producer_init(void)
{
    pr_info("==================================================\n");
    pr_info("symbol_producer loaded successfully!\n");
    pr_info("Exported symbols into kernel table:\n");
    pr_info("  - compute_device_checksum() [EXPORT_SYMBOL_GPL]\n");
    pr_info("  - global_driver_event_count = %d [EXPORT_SYMBOL_GPL]\n", global_driver_event_count);
    pr_info("Check /proc/kallsyms | grep compute_device_checksum\n");
    pr_info("==================================================\n");

    return 0;
}

static void __exit producer_exit(void)
{
    pr_info("symbol_producer unloading.\n");
    pr_info("Final global_driver_event_count: %d\n", global_driver_event_count);
    pr_info("Symbols unregistered from kernel symbol table.\n");
}

module_init(producer_init);
module_exit(producer_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Deepak Pandey");
MODULE_DESCRIPTION("Chapter 03: Kernel Symbol Producer Module");
MODULE_VERSION("1.0.0");
