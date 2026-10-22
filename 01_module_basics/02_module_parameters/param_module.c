// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file param_module.c
 * @brief Chapter 02: Kernel Module Parameters and Dynamic Sysfs Configuration.
 * @author Deepak Pandey
 *
 * This module demonstrates:
 *  - Standard load-time parameters (int, string, bool).
 *  - Array parameters passed via comma-separated values.
 *  - Sysfs permissions and runtime parameter modification.
 *  - Custom parameter notification callbacks using `struct kernel_param_ops`.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/string.h>

/* =========================================================================
 * 1. Standard Module Parameters
 * ========================================================================= */

// Default integer parameter (Read/Write by root in sysfs: 0644)
static int sample_rate = 1000;
module_param(sample_rate, int, 0644);
MODULE_PARM_DESC(sample_rate, "Sampling rate in Hz (default: 1000, sysfs: RW)");

// String parameter (Read-only in sysfs: 0444)
static char *device_tag = "DEFAULT_DEVICE";
module_param(device_tag, charp, 0444);
MODULE_PARM_DESC(device_tag, "Alphanumeric device tag identifier (sysfs: RO)");

// Boolean parameter (Load-time only, hidden from sysfs: 0000)
static bool verbose_logging = false;
module_param(verbose_logging, bool, 0);
MODULE_PARM_DESC(verbose_logging, "Enable verbose initialization output (0=no, 1=yes)");

/* =========================================================================
 * 2. Array Module Parameter
 * ========================================================================= */

#define MAX_ARRAY_ELEMS 5
static int threshold_levels[MAX_ARRAY_ELEMS] = { 10, 20, 30, 40, 50 };
static int threshold_count = MAX_ARRAY_ELEMS;

// Pass array and track number of elements provided by the user
module_param_array(threshold_levels, int, &threshold_count, 0644);
MODULE_PARM_DESC(threshold_levels, "Comma-separated threshold array up to 5 elements (sysfs: RW)");

/* =========================================================================
 * 3. Custom Parameter with Validation Callback (struct kernel_param_ops)
 * ========================================================================= */

// Target variable: Allowed range 1 to 200
static int speed_limit = 60;

/**
 * @brief Custom setter callback executed whenever user changes `speed_limit`.
 * Validates the value before accepting it.
 */
static int notify_param_set_speed(const char *val, const struct kernel_param *kp)
{
    int temp_val;
    int ret;

    ret = kstrtoint(val, 10, &temp_val);
    if (ret != 0) {
        pr_err("Invalid integer format for speed_limit: %s\n", val);
        return ret;
    }

    if (temp_val < 1 || temp_val > 200) {
        pr_err("Rejected speed_limit %d! Must be between 1 and 200.\n", temp_val);
        return -EINVAL; // Return invalid argument error to sysfs writer
    }

    pr_info(">>> speed_limit updated dynamically: %d -> %d <<<\n", speed_limit, temp_val);
    speed_limit = temp_val;
    return 0;
}

/**
 * @brief Custom getter callback executed when user reads `speed_limit` from sysfs.
 */
static int notify_param_get_speed(char *buffer, const struct kernel_param *kp)
{
    return scnprintf(buffer, PAGE_SIZE, "%d (valid range: 1-200)\n", speed_limit);
}

// Bind custom callbacks into kernel_param_ops
static const struct kernel_param_ops speed_param_ops = {
    .set = notify_param_set_speed,
    .get = notify_param_get_speed,
};

// Register parameter with custom ops (Read-Write: 0644)
module_param_cb(speed_limit, &speed_param_ops, &speed_limit, 0644);
MODULE_PARM_DESC(speed_limit, "Speed limit with range validation [1-200] (sysfs: RW)");

/* =========================================================================
 * 4. Module Lifecycle
 * ========================================================================= */

static int __init param_module_init(void)
{
    int i;

    pr_info("==================================================\n");
    pr_info("param_module loaded successfully!\n");
    pr_info("Parameters received at load-time:\n");
    pr_info("  - sample_rate:      %d Hz\n", sample_rate);
    pr_info("  - device_tag:       %s\n", device_tag);
    pr_info("  - verbose_logging:  %s\n", verbose_logging ? "ENABLED" : "DISABLED");
    pr_info("  - speed_limit:      %d\n", speed_limit);
    pr_info("  - threshold_levels: [%d elements]: ", threshold_count);
    for (i = 0; i < threshold_count; i++) {
        pr_cont("%d%s", threshold_levels[i], (i == threshold_count - 1) ? "" : ", ");
    }
    pr_cont("\n");
    pr_info("Check /sys/module/param_module/parameters/ to view & edit!\n");
    pr_info("==================================================\n");

    return 0;
}

static void __exit param_module_exit(void)
{
    pr_info("param_module unloading.\n");
    pr_info("Final speed_limit was: %d\n", speed_limit);
    pr_info("Goodbye!\n");
}

module_init(param_module_init);
module_exit(param_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Deepak Pandey");
MODULE_DESCRIPTION("Chapter 02: Module Parameters and Sysfs Interface");
MODULE_VERSION("1.0.0");
