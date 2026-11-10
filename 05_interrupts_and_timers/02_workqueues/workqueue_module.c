// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file workqueue_module.c
 * @brief Chapter 14: Deferred Work & Workqueues (schedule_work, alloc_workqueue, delayed_work)
 * @author Deepak Pandey
 *
 * This module demonstrates:
 *  - Bottom-Half execution using the standard system workqueue (system_wq).
 *  - Creating and managing private, dedicated workqueues with alloc_workqueue().
 *  - Scheduling timer-deferred work with struct delayed_work and schedule_delayed_work().
 *  - Safe synchronization, cancellation (cancel_delayed_work_sync), and flushing.
 *  - Demonstrating that workqueue functions execute in process context (can sleep, msleep).
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/ktime.h>
#include <linux/uaccess.h>
#include <linux/sched.h>

#include "workqueue_ioctl.h"

#define DEVICE_NAME "workqueue_demo"
#define CLASS_NAME  "workqueue_class"

/**
 * @brief Workqueue driver device state
 */
struct wq_demo_dev {
    dev_t dev_num;
    struct cdev cdev;
    struct class *dev_class;
    struct device *device;

    struct mutex lock;

    /* 1. Default system workqueue item */
    struct work_struct default_work;

    /* 2. Custom dedicated workqueue */
    struct workqueue_struct *custom_wq;
    struct work_struct custom_work;

    /* 3. Delayed work item */
    struct delayed_work dwork;

    /* Statistics */
    u32 default_work_count;
    u32 custom_work_count;
    u32 delayed_work_count;
    u64 last_queue_time_ns;
    u64 last_exec_time_ns;
    u32 last_worker_pid;
};

static struct wq_demo_dev g_wq_dev;

/**
 * @brief Worker function for system workqueue
 * Executes in process context (kworker thread), safe to sleep!
 */
static void default_work_handler(struct work_struct *work)
{
    struct wq_demo_dev *dev = container_of(work, struct wq_demo_dev, default_work);

    /* Demonstrate sleep capability */
    msleep(5);

    mutex_lock(&dev->lock);
    dev->default_work_count++;
    dev->last_exec_time_ns = ktime_get_ns();
    dev->last_worker_pid = current->pid;
    mutex_unlock(&dev->lock);

    pr_info("Default work executed by kworker [PID %d, Comm '%s']\n", current->pid, current->comm);
}

/**
 * @brief Worker function for dedicated private workqueue
 */
static void custom_work_handler(struct work_struct *work)
{
    struct wq_demo_dev *dev = container_of(work, struct wq_demo_dev, custom_work);

    mutex_lock(&dev->lock);
    dev->custom_work_count++;
    dev->last_exec_time_ns = ktime_get_ns();
    dev->last_worker_pid = current->pid;
    mutex_unlock(&dev->lock);

    pr_info("Custom workqueue item executed by [PID %d, Comm '%s']\n", current->pid, current->comm);
}

/**
 * @brief Worker function for delayed work
 */
static void delayed_work_handler(struct work_struct *work)
{
    struct delayed_work *dwork = to_delayed_work(work);
    struct wq_demo_dev *dev = container_of(dwork, struct wq_demo_dev, dwork);

    mutex_lock(&dev->lock);
    dev->delayed_work_count++;
    dev->last_exec_time_ns = ktime_get_ns();
    dev->last_worker_pid = current->pid;
    mutex_unlock(&dev->lock);

    pr_info("Delayed work executed after timer expiration by [PID %d]\n", current->pid);
}

static int wq_demo_open(struct inode *inode, struct file *file)
{
    return 0;
}

static int wq_demo_release(struct inode *inode, struct file *file)
{
    return 0;
}

static long wq_demo_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    struct wq_stats stats;
    u32 delay_ms = 0;

    switch (cmd) {
    case WQ_IOCTL_TRIGGER_DEFAULT:
        mutex_lock(&g_wq_dev.lock);
        g_wq_dev.last_queue_time_ns = ktime_get_ns();
        mutex_unlock(&g_wq_dev.lock);

        schedule_work(&g_wq_dev.default_work);
        return 0;

    case WQ_IOCTL_TRIGGER_CUSTOM:
        mutex_lock(&g_wq_dev.lock);
        g_wq_dev.last_queue_time_ns = ktime_get_ns();
        mutex_unlock(&g_wq_dev.lock);

        queue_work(g_wq_dev.custom_wq, &g_wq_dev.custom_work);
        return 0;

    case WQ_IOCTL_TRIGGER_DELAYED:
        if (copy_from_user(&delay_ms, (void __user *)arg, sizeof(delay_ms)))
            return -EFAULT;

        mutex_lock(&g_wq_dev.lock);
        g_wq_dev.last_queue_time_ns = ktime_get_ns();
        mutex_unlock(&g_wq_dev.lock);

        schedule_delayed_work(&g_wq_dev.dwork, msecs_to_jiffies(delay_ms));
        pr_info("Scheduled delayed work with delay of %u ms (%lu jiffies)\n",
                delay_ms, msecs_to_jiffies(delay_ms));
        return 0;

    case WQ_IOCTL_CANCEL_DELAYED:
        if (cancel_delayed_work_sync(&g_wq_dev.dwork)) {
            pr_info("Successfully canceled in-flight delayed work before it ran\n");
            return 0;
        } else {
            pr_info("Delayed work was not active or already finished\n");
            return 1;
        }

    case WQ_IOCTL_GET_STATS:
        mutex_lock(&g_wq_dev.lock);
        stats.default_work_count = g_wq_dev.default_work_count;
        stats.custom_work_count  = g_wq_dev.custom_work_count;
        stats.delayed_work_count = g_wq_dev.delayed_work_count;
        stats.last_queue_time_ns = g_wq_dev.last_queue_time_ns;
        stats.last_exec_time_ns  = g_wq_dev.last_exec_time_ns;
        stats.last_worker_pid    = g_wq_dev.last_worker_pid;
        mutex_unlock(&g_wq_dev.lock);

        if (copy_to_user((void __user *)arg, &stats, sizeof(stats)))
            return -EFAULT;
        return 0;

    default:
        return -ENOTTY;
    }
}

static const struct file_operations wq_demo_fops = {
    .owner          = THIS_MODULE,
    .open           = wq_demo_open,
    .release        = wq_demo_release,
    .unlocked_ioctl = wq_demo_ioctl,
};

static int __init wq_demo_init(void)
{
    int ret;

    pr_info("Initializing Deferred Work & Workqueues Driver (Chapter 14)\n");

    mutex_init(&g_wq_dev.lock);
    g_wq_dev.default_work_count = 0;
    g_wq_dev.custom_work_count  = 0;
    g_wq_dev.delayed_work_count = 0;
    g_wq_dev.last_queue_time_ns = 0;
    g_wq_dev.last_exec_time_ns  = 0;
    g_wq_dev.last_worker_pid    = 0;

    /* 1. Initialize work items */
    INIT_WORK(&g_wq_dev.default_work, default_work_handler);
    INIT_WORK(&g_wq_dev.custom_work, custom_work_handler);
    INIT_DELAYED_WORK(&g_wq_dev.dwork, delayed_work_handler);

    /* 2. Create dedicated single-thread custom workqueue */
    g_wq_dev.custom_wq = alloc_workqueue("custom_driver_wq", WQ_UNBOUND, 1);
    if (!g_wq_dev.custom_wq) {
        pr_err("Failed to create dedicated workqueue\n");
        return -ENOMEM;
    }

    /* 3. Register chrdev region */
    ret = alloc_chrdev_region(&g_wq_dev.dev_num, 0, 1, DEVICE_NAME);
    if (ret < 0) {
        pr_err("alloc_chrdev_region failed (%d)\n", ret);
        goto destroy_wq;
    }

    /* 4. Add cdev */
    cdev_init(&g_wq_dev.cdev, &wq_demo_fops);
    g_wq_dev.cdev.owner = THIS_MODULE;

    ret = cdev_add(&g_wq_dev.cdev, g_wq_dev.dev_num, 1);
    if (ret < 0) {
        pr_err("cdev_add failed (%d)\n", ret);
        goto unregister_chrdev;
    }

    /* 5. Create class */
    g_wq_dev.dev_class = class_create(CLASS_NAME);
    if (IS_ERR(g_wq_dev.dev_class)) {
        ret = PTR_ERR(g_wq_dev.dev_class);
        pr_err("class_create failed (%d)\n", ret);
        goto delete_cdev;
    }

    /* 6. Create device node */
    g_wq_dev.device = device_create(g_wq_dev.dev_class, NULL, g_wq_dev.dev_num,
                                    NULL, DEVICE_NAME);
    if (IS_ERR(g_wq_dev.device)) {
        ret = PTR_ERR(g_wq_dev.device);
        pr_err("device_create failed (%d)\n", ret);
        goto destroy_class;
    }

    pr_info("Workqueue driver loaded: /dev/%s active\n", DEVICE_NAME);
    return 0;

destroy_class:
    class_destroy(g_wq_dev.dev_class);
delete_cdev:
    cdev_del(&g_wq_dev.cdev);
unregister_chrdev:
    unregister_chrdev_region(g_wq_dev.dev_num, 1);
destroy_wq:
    destroy_workqueue(g_wq_dev.custom_wq);
    return ret;
}

static void __exit wq_demo_exit(void)
{
    pr_info("Unloading Workqueue Driver\n");

    /* Cancel any pending delayed work synchronously */
    cancel_delayed_work_sync(&g_wq_dev.dwork);

    /* Flush and drain custom workqueue before destroying */
    flush_workqueue(g_wq_dev.custom_wq);
    destroy_workqueue(g_wq_dev.custom_wq);

    device_destroy(g_wq_dev.dev_class, g_wq_dev.dev_num);
    class_destroy(g_wq_dev.dev_class);
    cdev_del(&g_wq_dev.cdev);
    unregister_chrdev_region(g_wq_dev.dev_num, 1);
    mutex_destroy(&g_wq_dev.lock);

    pr_info("Driver unloaded cleanly\n");
}

module_init(wq_demo_init);
module_exit(wq_demo_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Deepak Pandey");
MODULE_DESCRIPTION("Chapter 14: Deferred Work & Workqueues (schedule_work, alloc_workqueue, delayed_work)");
MODULE_VERSION("1.0");
