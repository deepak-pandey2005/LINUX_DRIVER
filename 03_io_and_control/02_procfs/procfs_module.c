// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file procfs_module.c
 * @brief Chapter 09: Procfs Diagnostic Interface & Kernel seq_file Subsystem.
 * @author Deepak Pandey
 *
 * This module demonstrates:
 *  - Creating custom directory nodes in /proc using proc_mkdir().
 *  - Modern Linux (5.6+) `struct proc_ops` interface.
 *  - High-performance, single/multi-page formatted output using the seq_file API (`single_open`, `seq_printf`).
 *  - Read-write proc entries for runtime parameter configuration.
 *  - Defensive user memory handling and parameter validation.
 *  - Clean, recursive removal with proc_remove() upon module unload.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/ktime.h>
#include <linux/string.h>

#define PROC_DIR_NAME    "driver_diagnostics"
#define PROC_STATUS_NAME "status"
#define PROC_CONFIG_NAME "config"

#define MAX_CONFIG_LEN   64

/**
 * @brief Driver operational diagnostics state
 */
struct driver_stats {
    struct mutex lock;
    u64 load_time_sec;
    u32 total_reads;
    u32 total_writes;
    int debug_level;              /* 0 = quiet, 1 = normal, 2 = verbose */
    char profile_name[MAX_CONFIG_LEN];
};

static struct driver_stats g_stats;
static struct proc_dir_entry *proc_dir_root;
static struct proc_dir_entry *proc_entry_status;
static struct proc_dir_entry *proc_entry_config;

/* =========================================================================
 * 1. Diagnostic Status Entry (/proc/driver_diagnostics/status)
 *    Uses seq_file for safe, scalable formatted text output.
 * ========================================================================= */

/**
 * @brief seq_file show callback for /proc/driver_diagnostics/status
 */
static int status_proc_show(struct seq_file *m, void *v)
{
    u64 uptime;

    mutex_lock(&g_stats.lock);
    g_stats.total_reads++;
    uptime = ktime_get_boottime_seconds() - g_stats.load_time_sec;

    seq_printf(m, "============================================\n");
    seq_printf(m, " Linux Driver Diagnostics (/proc Subsystem)\n");
    seq_printf(m, "============================================\n");
    seq_printf(m, "Driver Status        : ACTIVE\n");
    seq_printf(m, "Kernel Module        : %s\n", KBUILD_MODNAME);
    seq_printf(m, "Uptime (seconds)     : %llu\n", uptime);
    seq_printf(m, "Diagnostic Reads     : %u\n", g_stats.total_reads);
    seq_printf(m, "Config Writes        : %u\n", g_stats.total_writes);
    seq_printf(m, "Active Debug Level   : %d\n", g_stats.debug_level);
    seq_printf(m, "Hardware Profile     : %s\n", g_stats.profile_name);
    {
        int temp_mC = 36500 + (int)(g_stats.total_reads * 100);
        seq_printf(m, "Simulated Core Temp  : %d.%02d C\n", temp_mC / 1000, (temp_mC % 1000) / 10);
    }
    seq_printf(m, "============================================\n");

    mutex_unlock(&g_stats.lock);
    return 0;
}

static int status_proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, status_proc_show, NULL);
}

/**
 * Modern struct proc_ops (Linux 5.6+)
 */
static const struct proc_ops status_proc_ops = {
    .proc_open    = status_proc_open,
    .proc_read    = seq_read,
    .proc_lseek   = seq_lseek,
    .proc_release = single_release,
};

/* =========================================================================
 * 2. Configuration Entry (/proc/driver_diagnostics/config)
 *    Provides read/write interface for dynamically tuning driver options.
 * ========================================================================= */

static ssize_t config_proc_read(struct file *file, char __user *buf,
                               size_t count, loff_t *ppos)
{
    char kbuf[128];
    int len;

    mutex_lock(&g_stats.lock);
    len = snprintf(kbuf, sizeof(kbuf), "debug_level=%d\nprofile=%s\n",
                   g_stats.debug_level, g_stats.profile_name);
    mutex_unlock(&g_stats.lock);

    return simple_read_from_buffer(buf, count, ppos, kbuf, len);
}

static ssize_t config_proc_write(struct file *file, const char __user *buf,
                                size_t count, loff_t *ppos)
{
    char kbuf[MAX_CONFIG_LEN];
    size_t copy_len;
    char *key, *val, *p;

    if (count == 0)
        return 0;

    copy_len = min(count, sizeof(kbuf) - 1);
    if (copy_from_user(kbuf, buf, copy_len))
        return -EFAULT;

    kbuf[copy_len] = '\0';

    /* Strip trailing newline or carriage return */
    p = strchr(kbuf, '\n');
    if (p)
        *p = '\0';
    p = strchr(kbuf, '\r');
    if (p)
        *p = '\0';

    /* Parse key=value pairs */
    key = kbuf;
    val = strchr(kbuf, '=');
    if (!val) {
        pr_warn("Invalid config syntax. Expected key=value (e.g., debug_level=2)\n");
        return -EINVAL;
    }

    *val = '\0';
    val++;

    mutex_lock(&g_stats.lock);

    if (strcmp(key, "debug_level") == 0) {
        int level;
        if (kstrtoint(val, 10, &level) < 0 || level < 0 || level > 2) {
            mutex_unlock(&g_stats.lock);
            pr_warn("Invalid debug_level '%s' (allowed: 0, 1, 2)\n", val);
            return -EINVAL;
        }
        g_stats.debug_level = level;
        g_stats.total_writes++;
        pr_info("Updated debug_level to %d\n", level);
    } else if (strcmp(key, "profile") == 0) {
        if (strlen(val) == 0 || strlen(val) >= sizeof(g_stats.profile_name)) {
            mutex_unlock(&g_stats.lock);
            return -EINVAL;
        }
        strscpy(g_stats.profile_name, val, sizeof(g_stats.profile_name));
        g_stats.total_writes++;
        pr_info("Updated hardware profile to '%s'\n", val);
    } else {
        mutex_unlock(&g_stats.lock);
        pr_warn("Unknown config key '%s'\n", key);
        return -EINVAL;
    }

    mutex_unlock(&g_stats.lock);
    return count;
}

static const struct proc_ops config_proc_ops = {
    .proc_read  = config_proc_read,
    .proc_write = config_proc_write,
};

/* =========================================================================
 * Module Init / Exit
 * ========================================================================= */

static int __init procfs_module_init(void)
{
    pr_info("Initializing Procfs Diagnostic Subsystem\n");

    /* Initialize internal diagnostic state */
    mutex_init(&g_stats.lock);
    g_stats.load_time_sec = ktime_get_boottime_seconds();
    g_stats.total_reads = 0;
    g_stats.total_writes = 0;
    g_stats.debug_level = 1;
    strscpy(g_stats.profile_name, "default_high_performance", sizeof(g_stats.profile_name));

    /* Step 1: Create top-level directory /proc/driver_diagnostics */
    proc_dir_root = proc_mkdir(PROC_DIR_NAME, NULL);
    if (!proc_dir_root) {
        pr_err("proc_mkdir failed for /proc/%s\n", PROC_DIR_NAME);
        return -ENOMEM;
    }

    /* Step 2: Create read-only status node using seq_file */
    proc_entry_status = proc_create(PROC_STATUS_NAME, 0444, proc_dir_root, &status_proc_ops);
    if (!proc_entry_status) {
        pr_err("proc_create failed for %s\n", PROC_STATUS_NAME);
        proc_remove(proc_dir_root);
        return -ENOMEM;
    }

    /* Step 3: Create read-write config node */
    proc_entry_config = proc_create(PROC_CONFIG_NAME, 0666, proc_dir_root, &config_proc_ops);
    if (!proc_entry_config) {
        pr_err("proc_create failed for %s\n", PROC_CONFIG_NAME);
        proc_remove(proc_dir_root);
        return -ENOMEM;
    }

    pr_info("Procfs nodes created: /proc/%s/{%s, %s}\n",
            PROC_DIR_NAME, PROC_STATUS_NAME, PROC_CONFIG_NAME);
    return 0;
}

static void __exit procfs_module_exit(void)
{
    pr_info("Cleaning up Procfs Diagnostic Subsystem\n");

    /* Clean up entire subtree recursively */
    proc_remove(proc_dir_root);
    mutex_destroy(&g_stats.lock);

    pr_info("Procfs diagnostic directory cleanly removed.\n");
}

module_init(procfs_module_init);
module_exit(procfs_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Deepak Pandey");
MODULE_DESCRIPTION("Chapter 09: Modern proc_ops & seq_file Diagnostic Subsystem");
MODULE_VERSION("1.0");
