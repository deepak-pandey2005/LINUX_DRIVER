// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file major_minor_module.c
 * @brief Chapter 04: Major and Minor Device Number Allocation in Linux.
 * @author Deepak Pandey
 *
 * This module demonstrates:
 *  - The 32-bit `dev_t` data type.
 *  - Dynamic device number allocation with `alloc_chrdev_region()`.
 *  - Extracting major and minor numbers using `MAJOR()` and `MINOR()`.
 *  - Reconstructing numbers using `MKDEV()`.
 *  - Verifying the registered device in `/proc/devices`.
 *  - Safe cleanup with `unregister_chrdev_region()`.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>        // dev_t, alloc_chrdev_region, unregister_chrdev_region
#include <linux/kdev_t.h>    // MAJOR, MINOR, MKDEV

#define DEVICE_NAME "custom_char_dev"
#define BASE_MINOR  0
#define NUM_DEVICES 3       // Allocate 3 consecutive minor numbers (0, 1, 2)

// Holds the 32-bit major + minor device number combination
static dev_t dev_num;

static int __init char_num_init(void)
{
    int ret;
    int major_num, minor_num;

    pr_info("==================================================\n");
    pr_info("Initializing Character Device Number Lab...\n");

    /*
     * Dynamically request an available Major number and a range of Minor numbers.
     * Arguments:
     *   &dev_num: Output variable storing the allocated base dev_t.
     *   BASE_MINOR: Starting minor number (0).
     *   NUM_DEVICES: Count of consecutive minor numbers requested (3).
     *   DEVICE_NAME: Label that appears in /proc/devices.
     */
    ret = alloc_chrdev_region(&dev_num, BASE_MINOR, NUM_DEVICES, DEVICE_NAME);
    if (ret < 0) {
        pr_err("alloc_chrdev_region failed with error code: %d\n", ret);
        return ret;
    }

    major_num = MAJOR(dev_num);
    minor_num = MINOR(dev_num);

    pr_info("alloc_chrdev_region succeeded!\n");
    pr_info("  - Allocated dev_t (raw 32-bit): 0x%08x\n", dev_num);
    pr_info("  - Major Number: %d\n", major_num);
    pr_info("  - Base Minor Number: %d (Range: %d to %d)\n",
            minor_num, minor_num, minor_num + NUM_DEVICES - 1);
    pr_info("  - Registered Name in /proc/devices: '%s'\n", DEVICE_NAME);
    pr_info("Check: cat /proc/devices | grep %s\n", DEVICE_NAME);
    pr_info("==================================================\n");

    return 0;
}

static void __exit char_num_exit(void)
{
    pr_info("Unregistering character device numbers (Major %d)...\n", MAJOR(dev_num));

    // Release the allocated device number range back to the kernel
    unregister_chrdev_region(dev_num, NUM_DEVICES);

    pr_info("Device numbers released cleanly.\n");
}

module_init(char_num_init);
module_exit(char_num_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Deepak Pandey");
MODULE_DESCRIPTION("Chapter 04: Major and Minor Number Allocation");
MODULE_VERSION("1.0.0");
