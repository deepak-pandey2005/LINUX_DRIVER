// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file locking_module.c
 * @brief Chapter 11: Concurrency, Synchronization & Locking in Linux Drivers
 * @author Deepak Pandey
 *
 * This module demonstrates:
 *  - The catastrophic nature of race conditions on multi-core SMP systems when shared state is unprotected.
 *  - Lockless synchronization using atomic variables (atomic64_t).
 *  - Fast spinlocks (spinlock_t) for atomic/non-blocking contexts.
 *  - Sleepable mutexes (struct mutex) for process context.
 *  - Clean character device interface with IOCTL controls for benchmarking.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/atomic.h>

#include "locking_ioctl.h"

#define DEVICE_NAME "concurrency_bench"
#define CLASS_NAME  "concurrency_class"

/**
 * @brief Shared state tracking various synchronization mechanisms
 */
struct locking_benchmark_dev {
    dev_t dev_num;
    struct cdev cdev;
    struct class *dev_class;
    struct device *device;

    /* 1. Unsafe counter without protection (demonstrates race conditions) */
    u64 unsafe_counter;

    /* 2. Lockless atomic counter */
    atomic64_t atomic_counter;

    /* 3. Spinlock protected counter */
    spinlock_t spinlock;
    u64 spinlock_counter;

    /* 4. Mutex protected counter */
    struct mutex mutex;
    u64 mutex_counter;
};

static struct locking_benchmark_dev g_lock_dev;

static int locking_open(struct inode *inode, struct file *file)
{
    return 0;
}

static int locking_release(struct inode *inode, struct file *file)
{
    return 0;
}

static long locking_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    struct locking_results res;

    switch (cmd) {
    case LOCKING_IOCTL_RESET:
        /* Reset all counters under respective locks */
        g_lock_dev.unsafe_counter = 0;
        atomic64_set(&g_lock_dev.atomic_counter, 0);

        spin_lock(&g_lock_dev.spinlock);
        g_lock_dev.spinlock_counter = 0;
        spin_unlock(&g_lock_dev.spinlock);

        mutex_lock(&g_lock_dev.mutex);
        g_lock_dev.mutex_counter = 0;
        mutex_unlock(&g_lock_dev.mutex);

        pr_info("All benchmark counters reset to zero\n");
        return 0;

    case LOCKING_IOCTL_INC_UNSAFE:
        /*
         * Deliberate race condition: Read-Modify-Write operation without
         * synchronization. Under SMP contention, writes from parallel CPU
         * cores will collide and overwrite each other.
         */
        g_lock_dev.unsafe_counter++;
        return 0;

    case LOCKING_IOCTL_INC_ATOMIC:
        /* Hardware-level atomic bus cycle / LL-SC instruction */
        atomic64_inc(&g_lock_dev.atomic_counter);
        return 0;

    case LOCKING_IOCTL_INC_SPIN:
        /* Spinlock busy-wait synchronization */
        spin_lock(&g_lock_dev.spinlock);
        g_lock_dev.spinlock_counter++;
        spin_unlock(&g_lock_dev.spinlock);
        return 0;

    case LOCKING_IOCTL_INC_MUTEX:
        /* Mutex sleepable synchronization */
        if (mutex_lock_interruptible(&g_lock_dev.mutex))
            return -ERESTARTSYS;
        g_lock_dev.mutex_counter++;
        mutex_unlock(&g_lock_dev.mutex);
        return 0;

    case LOCKING_IOCTL_GET_RESULTS:
        /* Gather current values safely */
        res.unsafe_val = g_lock_dev.unsafe_counter;
        res.atomic_val = atomic64_read(&g_lock_dev.atomic_counter);

        spin_lock(&g_lock_dev.spinlock);
        res.spinlock_val = g_lock_dev.spinlock_counter;
        spin_unlock(&g_lock_dev.spinlock);

        mutex_lock(&g_lock_dev.mutex);
        res.mutex_val = g_lock_dev.mutex_counter;
        mutex_unlock(&g_lock_dev.mutex);

        if (copy_to_user((void __user *)arg, &res, sizeof(res)))
            return -EFAULT;

        return 0;

    default:
        pr_warn("Unrecognized IOCTL cmd: 0x%x\n", cmd);
        return -ENOTTY;
    }
}

static const struct file_operations locking_fops = {
    .owner          = THIS_MODULE,
    .open           = locking_open,
    .release        = locking_release,
    .unlocked_ioctl = locking_ioctl,
};

static int __init locking_init(void)
{
    int ret;

    pr_info("Initializing Concurrency & Synchronization Benchmark (Chapter 11)\n");

    /* Initialize synchronization primitives */
    g_lock_dev.unsafe_counter = 0;
    atomic64_set(&g_lock_dev.atomic_counter, 0);
    spin_lock_init(&g_lock_dev.spinlock);
    g_lock_dev.spinlock_counter = 0;
    mutex_init(&g_lock_dev.mutex);
    g_lock_dev.mutex_counter = 0;

    /* 1. Allocate dynamic character device region */
    ret = alloc_chrdev_region(&g_lock_dev.dev_num, 0, 1, DEVICE_NAME);
    if (ret < 0) {
        pr_err("Failed to allocate chrdev region (ret = %d)\n", ret);
        return ret;
    }

    /* 2. Initialize and add struct cdev */
    cdev_init(&g_lock_dev.cdev, &locking_fops);
    g_lock_dev.cdev.owner = THIS_MODULE;

    ret = cdev_add(&g_lock_dev.cdev, g_lock_dev.dev_num, 1);
    if (ret < 0) {
        pr_err("Failed to add cdev (ret = %d)\n", ret);
        goto unregister_chrdev;
    }

    /* 3. Create device class */
    g_lock_dev.dev_class = class_create(CLASS_NAME);
    if (IS_ERR(g_lock_dev.dev_class)) {
        ret = PTR_ERR(g_lock_dev.dev_class);
        pr_err("Failed to create device class (ret = %d)\n", ret);
        goto delete_cdev;
    }

    /* 4. Create device node in /dev */
    g_lock_dev.device = device_create(g_lock_dev.dev_class, NULL, g_lock_dev.dev_num,
                                      NULL, DEVICE_NAME);
    if (IS_ERR(g_lock_dev.device)) {
        ret = PTR_ERR(g_lock_dev.device);
        pr_err("Failed to create device node /dev/%s (ret = %d)\n", DEVICE_NAME, ret);
        goto destroy_class;
    }

    pr_info("Driver loaded: /dev/%s registered (Major: %d, Minor: %d)\n",
            DEVICE_NAME, MAJOR(g_lock_dev.dev_num), MINOR(g_lock_dev.dev_num));
    return 0;

destroy_class:
    class_destroy(g_lock_dev.dev_class);
delete_cdev:
    cdev_del(&g_lock_dev.cdev);
unregister_chrdev:
    unregister_chrdev_region(g_lock_dev.dev_num, 1);
    return ret;
}

static void __exit locking_exit(void)
{
    pr_info("Unloading Concurrency Benchmark Driver\n");

    device_destroy(g_lock_dev.dev_class, g_lock_dev.dev_num);
    class_destroy(g_lock_dev.dev_class);
    cdev_del(&g_lock_dev.cdev);
    unregister_chrdev_region(g_lock_dev.dev_num, 1);
    mutex_destroy(&g_lock_dev.mutex);

    pr_info("Driver unloaded cleanly\n");
}

module_init(locking_init);
module_exit(locking_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Deepak Pandey");
MODULE_DESCRIPTION("Chapter 11: Concurrency, Synchronization & Locking (Atomics, Spinlocks, Mutexes)");
MODULE_VERSION("1.0");

