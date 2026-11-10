// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file timer_module.c
 * @brief Chapter 15: Kernel Timers, Jiffies & Delays (timer_list, hrtimer, delays)
 * @author Deepak Pandey
 *
 * This module demonstrates:
 *  - Low-resolution kernel timers with struct timer_list, timer_setup(), and mod_timer().
 *  - High-resolution timers (hrtimer) with hrtimer_setup() and sub-millisecond precision.
 *  - Jiffies, HZ calculations, and time conversions (msecs_to_jiffies).
 *  - Safe synchronization using spinlocks for timer callbacks.
 *  - Synchronous timer cancellation with timer_shutdown_sync() and hrtimer_cancel().
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/timer.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>
#include <linux/jiffies.h>

#include "timer_ioctl.h"

#define DEVICE_NAME "kernel_timers"
#define CLASS_NAME  "timer_class"

/**
 * @brief Driver timer state
 */
struct timer_demo_dev {
    dev_t dev_num;
    struct cdev cdev;
    struct class *dev_class;
    struct device *device;

    spinlock_t lock;

    /* 1. Low-resolution standard timer */
    struct timer_list std_timer;
    u32 std_period_ms;
    bool std_timer_active;
    u32 std_timer_ticks;
    u64 last_std_timestamp_ns;

    /* 2. High-resolution timer */
    struct hrtimer hr_timer;
    ktime_t hr_period;
    bool hr_timer_active;
    u32 hr_timer_ticks;
    u64 last_hr_timestamp_ns;
};

static struct timer_demo_dev g_timer_dev;

/**
 * @brief Standard low-resolution timer callback
 * Runs in softirq context (TIMER_SOFTIRQ) - CANNOT SLEEP!
 */
static void std_timer_callback(struct timer_list *t)
{
    struct timer_demo_dev *dev = container_of(t, struct timer_demo_dev, std_timer);
    unsigned long flags;

    spin_lock_irqsave(&dev->lock, flags);
    dev->std_timer_ticks++;
    dev->last_std_timestamp_ns = ktime_get_ns();

    if (dev->std_timer_active) {
        /* Rearm timer for periodic execution */
        mod_timer(&dev->std_timer, jiffies + msecs_to_jiffies(dev->std_period_ms));
    }
    spin_unlock_irqrestore(&dev->lock, flags);
}

/**
 * @brief High-resolution timer callback
 * Runs in hard/soft irq context - nanosecond precision
 */
static enum hrtimer_restart hr_timer_callback(struct hrtimer *timer)
{
    struct timer_demo_dev *dev = container_of(timer, struct timer_demo_dev, hr_timer);
    unsigned long flags;

    spin_lock_irqsave(&dev->lock, flags);
    dev->hr_timer_ticks++;
    dev->last_hr_timestamp_ns = ktime_get_ns();

    if (dev->hr_timer_active) {
        hrtimer_forward_now(timer, dev->hr_period);
        spin_unlock_irqrestore(&dev->lock, flags);
        return HRTIMER_RESTART;
    }

    spin_unlock_irqrestore(&dev->lock, flags);
    return HRTIMER_NORESTART;
}

static int timer_demo_open(struct inode *inode, struct file *file)
{
    return 0;
}

static int timer_demo_release(struct inode *inode, struct file *file)
{
    return 0;
}

static long timer_demo_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    unsigned long flags;
    u32 period = 0;
    struct timer_stats stats;

    switch (cmd) {
    case TIMER_IOCTL_START_STD:
        if (copy_from_user(&period, (void __user *)arg, sizeof(period)))
            return -EFAULT;
        if (period == 0)
            return -EINVAL;

        spin_lock_irqsave(&g_timer_dev.lock, flags);
        g_timer_dev.std_period_ms = period;
        g_timer_dev.std_timer_ticks = 0;
        g_timer_dev.std_timer_active = true;
        mod_timer(&g_timer_dev.std_timer, jiffies + msecs_to_jiffies(period));
        spin_unlock_irqrestore(&g_timer_dev.lock, flags);

        pr_info("Started standard timer with period %u ms (HZ = %d)\n", period, HZ);
        return 0;

    case TIMER_IOCTL_STOP_STD:
        spin_lock_irqsave(&g_timer_dev.lock, flags);
        g_timer_dev.std_timer_active = false;
        spin_unlock_irqrestore(&g_timer_dev.lock, flags);

        timer_shutdown_sync(&g_timer_dev.std_timer);
        pr_info("Stopped standard timer\n");
        return 0;

    case TIMER_IOCTL_START_HR:
        if (copy_from_user(&period, (void __user *)arg, sizeof(period)))
            return -EFAULT;
        if (period == 0)
            return -EINVAL;

        spin_lock_irqsave(&g_timer_dev.lock, flags);
        g_timer_dev.hr_period = ktime_set(0, period * 1000); /* microseconds to nanoseconds */
        g_timer_dev.hr_timer_ticks = 0;
        g_timer_dev.hr_timer_active = true;
        hrtimer_start(&g_timer_dev.hr_timer, g_timer_dev.hr_period, HRTIMER_MODE_REL);
        spin_unlock_irqrestore(&g_timer_dev.lock, flags);

        pr_info("Started high-resolution timer with period %u us\n", period);
        return 0;

    case TIMER_IOCTL_STOP_HR:
        spin_lock_irqsave(&g_timer_dev.lock, flags);
        g_timer_dev.hr_timer_active = false;
        spin_unlock_irqrestore(&g_timer_dev.lock, flags);

        hrtimer_cancel(&g_timer_dev.hr_timer);
        pr_info("Stopped high-resolution timer\n");
        return 0;

    case TIMER_IOCTL_GET_STATS:
        spin_lock_irqsave(&g_timer_dev.lock, flags);
        stats.std_timer_ticks = g_timer_dev.std_timer_ticks;
        stats.hr_timer_ticks = g_timer_dev.hr_timer_ticks;
        stats.last_std_timestamp_ns = g_timer_dev.last_std_timestamp_ns;
        stats.last_hr_timestamp_ns = g_timer_dev.last_hr_timestamp_ns;
        stats.hz_rate = HZ;
        spin_unlock_irqrestore(&g_timer_dev.lock, flags);

        if (copy_to_user((void __user *)arg, &stats, sizeof(stats)))
            return -EFAULT;
        return 0;

    default:
        return -ENOTTY;
    }
}

static const struct file_operations timer_demo_fops = {
    .owner          = THIS_MODULE,
    .open           = timer_demo_open,
    .release        = timer_demo_release,
    .unlocked_ioctl = timer_demo_ioctl,
};

static int __init timer_demo_init(void)
{
    int ret;

    pr_info("Initializing Kernel Timers Driver (Chapter 15)\n");

    spin_lock_init(&g_timer_dev.lock);
    g_timer_dev.std_timer_active = false;
    g_timer_dev.std_timer_ticks = 0;
    g_timer_dev.last_std_timestamp_ns = 0;
    g_timer_dev.hr_timer_active = false;
    g_timer_dev.hr_timer_ticks = 0;
    g_timer_dev.last_hr_timestamp_ns = 0;

    /* 1. Setup standard timer */
    timer_setup(&g_timer_dev.std_timer, std_timer_callback, 0);

    /* 2. Setup high-resolution timer */
    hrtimer_setup(&g_timer_dev.hr_timer, hr_timer_callback, CLOCK_MONOTONIC, HRTIMER_MODE_REL);

    /* 3. Register chrdev region */
    ret = alloc_chrdev_region(&g_timer_dev.dev_num, 0, 1, DEVICE_NAME);
    if (ret < 0) {
        pr_err("alloc_chrdev_region failed (%d)\n", ret);
        return ret;
    }

    /* 4. Add cdev */
    cdev_init(&g_timer_dev.cdev, &timer_demo_fops);
    g_timer_dev.cdev.owner = THIS_MODULE;

    ret = cdev_add(&g_timer_dev.cdev, g_timer_dev.dev_num, 1);
    if (ret < 0) {
        pr_err("cdev_add failed (%d)\n", ret);
        goto unregister_chrdev;
    }

    /* 5. Create class */
    g_timer_dev.dev_class = class_create(CLASS_NAME);
    if (IS_ERR(g_timer_dev.dev_class)) {
        ret = PTR_ERR(g_timer_dev.dev_class);
        pr_err("class_create failed (%d)\n", ret);
        goto delete_cdev;
    }

    /* 6. Create device node */
    g_timer_dev.device = device_create(g_timer_dev.dev_class, NULL, g_timer_dev.dev_num,
                                       NULL, DEVICE_NAME);
    if (IS_ERR(g_timer_dev.device)) {
        ret = PTR_ERR(g_timer_dev.device);
        pr_err("device_create failed (%d)\n", ret);
        goto destroy_class;
    }

    pr_info("Timer driver loaded: /dev/%s active (Kernel HZ = %d)\n", DEVICE_NAME, HZ);
    return 0;

destroy_class:
    class_destroy(g_timer_dev.dev_class);
delete_cdev:
    cdev_del(&g_timer_dev.cdev);
unregister_chrdev:
    unregister_chrdev_region(g_timer_dev.dev_num, 1);
    return ret;
}

static void __exit timer_demo_exit(void)
{
    pr_info("Unloading Kernel Timers Driver\n");

    /* Stop timers synchronously */
    g_timer_dev.std_timer_active = false;
    g_timer_dev.hr_timer_active = false;
    timer_shutdown_sync(&g_timer_dev.std_timer);
    hrtimer_cancel(&g_timer_dev.hr_timer);

    device_destroy(g_timer_dev.dev_class, g_timer_dev.dev_num);
    class_destroy(g_timer_dev.dev_class);
    cdev_del(&g_timer_dev.cdev);
    unregister_chrdev_region(g_timer_dev.dev_num, 1);

    pr_info("Driver unloaded cleanly\n");
}

module_init(timer_demo_init);
module_exit(timer_demo_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Deepak Pandey");
MODULE_DESCRIPTION("Chapter 15: Kernel Timers, Jiffies & Delays (timer_list, hrtimer, delays)");
MODULE_VERSION("1.0");
