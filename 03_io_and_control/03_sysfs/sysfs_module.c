// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file sysfs_module.c
 * @brief Chapter 10: Sysfs & Kernel Objects (kobject, attributes, attribute groups)
 * @author Deepak Pandey
 *
 * This module demonstrates:
 *  - Creating custom sysfs directory hierarchies with kobject_create_and_add().
 *  - Creating read-only, read-write, and write-only attributes.
 *  - Grouping attributes cleanly using struct attribute_group and sysfs_create_group().
 *  - Standard sysfs input parsing and validation using kstrtouint() and sysfs_streq().
 *  - Adhering to the Linux sysfs "one value per file" design philosophy.
 *  - SMP thread safety using mutex synchronization.
 *  - Clean reference counting and teardown with kobject_put().
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/string.h>
#include <linux/mutex.h>
#include <linux/ktime.h>

#define SYSFS_DIR_NAME "telemetry_subsystem"
#define MODE_STR_LEN   16

enum dev_power_mode {
    MODE_LOW_POWER = 0,
    MODE_NORMAL    = 1,
    MODE_OVERCLOCK = 2,
};

static const char * const mode_names[] = {
    [MODE_LOW_POWER] = "LOW_POWER",
    [MODE_NORMAL]    = "NORMAL",
    [MODE_OVERCLOCK] = "OVERCLOCK",
};

/**
 * @brief Hardware telemetry device state
 */
struct device_telemetry {
    struct mutex lock;
    enum dev_power_mode mode;
    unsigned int baud_rate;
    u64 load_time_sec;
    unsigned int read_count;
    unsigned int write_count;
    unsigned int error_count;
};

static struct device_telemetry g_telemetry;
static struct kobject *telemetry_kobj;

/* =========================================================================
 * Sysfs Attribute Callbacks
 * ========================================================================= */

/**
 * 1. device_mode attribute (RW)
 *    Exposes the active operational mode: LOW_POWER, NORMAL, OVERCLOCK.
 */
static ssize_t device_mode_show(struct kobject *kobj, struct kobj_attribute *attr,
                                char *buf)
{
    ssize_t ret;

    mutex_lock(&g_telemetry.lock);
    g_telemetry.read_count++;
    ret = sysfs_emit(buf, "%s\n", mode_names[g_telemetry.mode]);
    mutex_unlock(&g_telemetry.lock);

    return ret;
}

static ssize_t device_mode_store(struct kobject *kobj, struct kobj_attribute *attr,
                                 const char *buf, size_t count)
{
    enum dev_power_mode new_mode;
    int found = 0;

    mutex_lock(&g_telemetry.lock);
    g_telemetry.write_count++;

    if (sysfs_streq(buf, "LOW_POWER")) {
        new_mode = MODE_LOW_POWER;
        found = 1;
    } else if (sysfs_streq(buf, "NORMAL")) {
        new_mode = MODE_NORMAL;
        found = 1;
    } else if (sysfs_streq(buf, "OVERCLOCK")) {
        new_mode = MODE_OVERCLOCK;
        found = 1;
    }

    if (!found) {
        g_telemetry.error_count++;
        mutex_unlock(&g_telemetry.lock);
        pr_warn("device_mode: invalid mode string: '%.*s'\n", (int)count, buf);
        return -EINVAL;
    }

    g_telemetry.mode = new_mode;
    mutex_unlock(&g_telemetry.lock);

    pr_info("device_mode updated to: %s\n", mode_names[new_mode]);
    return count;
}

static struct kobj_attribute dev_attr_device_mode =
    __ATTR(device_mode, 0644, device_mode_show, device_mode_store);

/**
 * 2. baud_rate attribute (RW)
 *    Accepts standard UART baud rates: 9600, 19200, 38400, 57600, 115200.
 */
static ssize_t baud_rate_show(struct kobject *kobj, struct kobj_attribute *attr,
                              char *buf)
{
    ssize_t ret;

    mutex_lock(&g_telemetry.lock);
    g_telemetry.read_count++;
    ret = sysfs_emit(buf, "%u\n", g_telemetry.baud_rate);
    mutex_unlock(&g_telemetry.lock);

    return ret;
}

static ssize_t baud_rate_store(struct kobject *kobj, struct kobj_attribute *attr,
                               const char *buf, size_t count)
{
    unsigned int val;
    int err;

    mutex_lock(&g_telemetry.lock);
    g_telemetry.write_count++;

    err = kstrtouint(buf, 10, &val);
    if (err) {
        g_telemetry.error_count++;
        mutex_unlock(&g_telemetry.lock);
        return err;
    }

    /* Validate permissible baud rates */
    if (val != 9600 && val != 19200 && val != 38400 && val != 57600 && val != 115200) {
        g_telemetry.error_count++;
        mutex_unlock(&g_telemetry.lock);
        pr_warn("baud_rate: invalid baud rate %u (allowed: 9600, 19200, 38400, 57600, 115200)\n", val);
        return -EINVAL;
    }

    g_telemetry.baud_rate = val;
    mutex_unlock(&g_telemetry.lock);

    pr_info("baud_rate updated to: %u\n", val);
    return count;
}

static struct kobj_attribute dev_attr_baud_rate =
    __ATTR(baud_rate, 0644, baud_rate_show, baud_rate_store);

/**
 * 3. uptime_seconds attribute (RO)
 *    Returns the uptime in seconds since module initialization.
 */
static ssize_t uptime_seconds_show(struct kobject *kobj, struct kobj_attribute *attr,
                                   char *buf)
{
    u64 uptime;
    ssize_t ret;

    mutex_lock(&g_telemetry.lock);
    g_telemetry.read_count++;
    uptime = ktime_get_boottime_seconds() - g_telemetry.load_time_sec;
    ret = sysfs_emit(buf, "%llu\n", uptime);
    mutex_unlock(&g_telemetry.lock);

    return ret;
}

static struct kobj_attribute dev_attr_uptime_seconds =
    __ATTR_RO(uptime_seconds);

/**
 * 4. stats attribute (RO)
 *    Provides structured telemetry summary.
 */
static ssize_t stats_show(struct kobject *kobj, struct kobj_attribute *attr,
                          char *buf)
{
    ssize_t ret;

    mutex_lock(&g_telemetry.lock);
    g_telemetry.read_count++;
    ret = sysfs_emit(buf,
                     "reads=%u writes=%u errors=%u mode=%s baud=%u\n",
                     g_telemetry.read_count,
                     g_telemetry.write_count,
                     g_telemetry.error_count,
                     mode_names[g_telemetry.mode],
                     g_telemetry.baud_rate);
    mutex_unlock(&g_telemetry.lock);

    return ret;
}

static struct kobj_attribute dev_attr_stats =
    __ATTR_RO(stats);

/**
 * 5. reset attribute (WO)
 *    Writing "1" resets read/write/error statistics and restores defaults.
 */
static ssize_t reset_store(struct kobject *kobj, struct kobj_attribute *attr,
                           const char *buf, size_t count)
{
    unsigned int val;
    int err;

    err = kstrtouint(buf, 10, &val);
    if (err)
        return err;

    if (val != 1) {
        pr_warn("reset: write '1' to trigger reset (received %u)\n", val);
        return -EINVAL;
    }

    mutex_lock(&g_telemetry.lock);
    g_telemetry.mode        = MODE_NORMAL;
    g_telemetry.baud_rate   = 115200;
    g_telemetry.read_count  = 0;
    g_telemetry.write_count = 0;
    g_telemetry.error_count = 0;
    mutex_unlock(&g_telemetry.lock);

    pr_info("Driver telemetry statistics and settings reset to defaults\n");
    return count;
}

static struct kobj_attribute dev_attr_reset =
    __ATTR(reset, 0200, NULL, reset_store);

/* =========================================================================
 * Attribute Group Definition
 * ========================================================================= */

static struct attribute *telemetry_attrs[] = {
    &dev_attr_device_mode.attr,
    &dev_attr_baud_rate.attr,
    &dev_attr_uptime_seconds.attr,
    &dev_attr_stats.attr,
    &dev_attr_reset.attr,
    NULL, /* Sentinel */
};

static const struct attribute_group telemetry_attr_group = {
    .attrs = telemetry_attrs,
};

/* =========================================================================
 * Module Init & Exit
 * ========================================================================= */

static int __init sysfs_telemetry_init(void)
{
    int ret;

    pr_info("Initializing Sysfs Telemetry Driver (Chapter 10)\n");

    /* Initialize device state */
    mutex_init(&g_telemetry.lock);
    g_telemetry.mode          = MODE_NORMAL;
    g_telemetry.baud_rate     = 115200;
    g_telemetry.load_time_sec = ktime_get_boottime_seconds();
    g_telemetry.read_count    = 0;
    g_telemetry.write_count   = 0;
    g_telemetry.error_count   = 0;

    /* Create kobject under /sys/kernel/ */
    telemetry_kobj = kobject_create_and_add(SYSFS_DIR_NAME, kernel_kobj);
    if (!telemetry_kobj) {
        pr_err("Failed to create kobject /sys/kernel/%s\n", SYSFS_DIR_NAME);
        return -ENOMEM;
    }

    /* Register attribute group atomically */
    ret = sysfs_create_group(telemetry_kobj, &telemetry_attr_group);
    if (ret) {
        pr_err("Failed to create sysfs attribute group (ret = %d)\n", ret);
        kobject_put(telemetry_kobj);
        return ret;
    }

    pr_info("Sysfs telemetry registered successfully at /sys/kernel/%s\n", SYSFS_DIR_NAME);
    return 0;
}

static void __exit sysfs_telemetry_exit(void)
{
    pr_info("Unloading Sysfs Telemetry Driver\n");

    if (telemetry_kobj) {
        sysfs_remove_group(telemetry_kobj, &telemetry_attr_group);
        kobject_put(telemetry_kobj);
        telemetry_kobj = NULL;
    }

    mutex_destroy(&g_telemetry.lock);
    pr_info("Sysfs telemetry unloaded cleanly\n");
}

module_init(sysfs_telemetry_init);
module_exit(sysfs_telemetry_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Deepak Pandey");
MODULE_DESCRIPTION("Chapter 10: Sysfs & Kernel Objects Diagnostic Telemetry Interface");
MODULE_VERSION("1.0");

