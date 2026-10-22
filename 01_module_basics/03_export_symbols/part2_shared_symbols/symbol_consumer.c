// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file symbol_consumer.c
 * @brief Chapter 03 (Part 2): Kernel Symbol Consumer Module.
 * @author Deepak Pandey
 *
 * Demonstrates importing and invoking symbols exported by another module.
 * Requires `symbol_producer.ko` to be loaded first!
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include "symbol_producer.h"

static int __init consumer_init(void)
{
    const u8 test_payload[] = "LINUX_DRIVER_PAYLOAD";
    u32 checksum;

    pr_info("==================================================\n");
    pr_info("symbol_consumer loaded successfully!\n");
    pr_info("Invoking exported symbols from symbol_producer:\n");

    // 1. Read and modify exported global variable
    pr_info("  - Current global_driver_event_count: %d\n", global_driver_event_count);
    global_driver_event_count += 5;
    pr_info("  - Incremented global_driver_event_count to: %d\n", global_driver_event_count);

    // 2. Call exported function
    checksum = compute_device_checksum(test_payload, sizeof(test_payload));
    pr_info("  - Calculated payload checksum: 0x%08x\n", checksum);
    pr_info("Notice: 'lsmod | grep symbol_producer' now shows Used By: symbol_consumer!\n");
    pr_info("==================================================\n");

    return 0;
}

static void __exit consumer_exit(void)
{
    pr_info("symbol_consumer unloading.\n");
    pr_info("Releasing dependency on symbol_producer.\n");
}

module_init(consumer_init);
module_exit(consumer_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Deepak Pandey");
MODULE_DESCRIPTION("Chapter 03: Kernel Symbol Consumer Module");
MODULE_VERSION("1.0.0");
