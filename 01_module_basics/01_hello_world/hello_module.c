// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file hello_module.c
 * @brief Beginner Linux Kernel Module: Hello World and Module Lifecycle.
 * @author Deepak Pandey
 *
 * This module demonstrates the fundamental architecture of a Linux Loadable
 * Kernel Module (LKM), including:
 *  - Proper header inclusions.
 *  - Module initialization and cleanup hooks.
 *  - The __init and __exit compiler attributes for memory optimization.
 *  - Modern pr_fmt and pr_* logging vs. legacy raw printk.
 *  - Essential module metadata macros.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/init.h>          // Macros: __init, __exit, module_init, module_exit
#include <linux/module.h>        // Core module support: MODULE_LICENSE, etc.
#include <linux/kernel.h>        // Kernel utilities: pr_info, pr_err, etc.
#include <generated/utsrelease.h>// Kernel release string: UTS_RELEASE

/**
 * @brief Module Initialization function.
 *
 * The __init macro places this function into the `.init.text` ELF section.
 * Once initialization is complete, the kernel frees this memory back to the
 * system page allocator.
 *
 * @return 0 on success, or a negative error code (e.g., -ENOMEM, -EINVAL).
 */
static int __init hello_init(void)
{
    pr_info("========================================\n");
    pr_info("Hello from the Linux Kernel!\n");
    pr_info("Module loaded successfully into kernel space.\n");
    pr_info("Running under kernel version: %s\n", UTS_RELEASE);
    pr_info("========================================\n");

    return 0; // Returning 0 tells the kernel the module loaded successfully.
}

/**
 * @brief Module Exit / Cleanup function.
 *
 * The __exit macro places this function into the `.exit.text` ELF section.
 * If this driver were built statically into the kernel (`obj-y`), the exit
 * function would be discarded at compile time, saving precious binary space.
 */
static void __exit hello_exit(void)
{
    pr_info("Goodbye from the Linux Kernel!\n");
    pr_info("Module unloaded cleanly. Memory reclaimed.\n");
}

/*
 * Register the initialization and exit entry points.
 * These macros tell the kernel which functions to execute during `insmod`
 * and `rmmod`.
 */
module_init(hello_init);
module_exit(hello_exit);

/*
 * Module Metadata.
 * Used by tools like `modinfo` to query information about the module.
 */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Deepak Pandey");
MODULE_DESCRIPTION("A robust introductory Linux Loadable Kernel Module (LKM)");
MODULE_VERSION("1.0.0");
