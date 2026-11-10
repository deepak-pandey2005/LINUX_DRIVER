// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file interrupt_module.c
 * @brief Chapter 13: Hardware & Shared Interrupt Handling (request_irq, IRQF_SHARED, Top-Half ISR)
 * @author Deepak Pandey
 *
 * This module demonstrates:
 *  - Registering shared hardware interrupt handlers with request_irq() and IRQF_SHARED.
 *  - Fast Top-Half execution and irqreturn_t conventions (IRQ_HANDLED vs IRQ_NONE).
 *  - Spinlock-protected interrupt counters and multi-core CPU tracking (smp_processor_id).
 *  - Fallback to simulated software interrupts when hardware lines are busy or unavailable.
 *  - Clean interrupt line detachment with free_irq().
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/ktime.h>
#include <linux/uaccess.h>
#include <linux/smp.h>

#include "interrupt_ioctl.h"

#define DEVICE_NAME "irq_demo"
#define CLASS_NAME  "irq_class"

/* Module parameter: default to 1 (often keyboard / i8042) or 0 (pure simulation) */
static int irq_num = 1;
module_param(irq_num, int, 0444);
MODULE_PARM_DESC(irq_num, "Hardware IRQ line to attach to with IRQF_SHARED (default=1, 0=simulation)");

/**
 * @brief Shared driver device state
 */
struct irq_demo_dev {
    dev_t dev_num;
    struct cdev cdev;
    struct class *dev_class;
    struct device *device;

    /* Hardware interrupt state */
    int active_irq;
    bool hw_registered;

    /* Interrupt statistics */
    spinlock_t lock;
    u64 interrupt_count;
    u64 last_interrupt_ns;
    u32 last_cpu;
};

static struct irq_demo_dev g_irq_dev;

/**
 * @brief Top-Half Interrupt Service Routine (ISR)
 *
 * Rules:
 *  1. Must execute fast (sub-microsecond).
 *  2. NEVER sleep, block, or call schedule().
 *  3. Return IRQ_HANDLED if our device caused it, or IRQ_NONE if shared and not ours.
 */
static irqreturn_t isr_top_half(int irq, void *dev_id)
{
    struct irq_demo_dev *dev = (struct irq_demo_dev *)dev_id;
    unsigned long flags;

    spin_lock_irqsave(&dev->lock, flags);
    dev->interrupt_count++;
    dev->last_interrupt_ns = ktime_get_ns();
    dev->last_cpu = smp_processor_id();
    spin_unlock_irqrestore(&dev->lock, flags);

    return IRQ_HANDLED;
}

static int irq_demo_open(struct inode *inode, struct file *file)
{
    return 0;
}

static int irq_demo_release(struct inode *inode, struct file *file)
{
    return 0;
}

static long irq_demo_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    unsigned long flags;
    struct irq_stats stats;

    switch (cmd) {
    case IRQ_IOCTL_TRIGGER_SOFTWARE:
        /* Simulate an interrupt event */
        isr_top_half(g_irq_dev.active_irq, &g_irq_dev);
        return 0;

    case IRQ_IOCTL_GET_STATS:
        spin_lock_irqsave(&g_irq_dev.lock, flags);
        stats.irq_num = g_irq_dev.active_irq;
        stats.is_hardware_registered = g_irq_dev.hw_registered ? 1 : 0;
        stats.total_interrupts = g_irq_dev.interrupt_count;
        stats.last_interrupt_ns = g_irq_dev.last_interrupt_ns;
        stats.last_cpu = g_irq_dev.last_cpu;
        spin_unlock_irqrestore(&g_irq_dev.lock, flags);

        if (copy_to_user((void __user *)arg, &stats, sizeof(stats)))
            return -EFAULT;
        return 0;

    case IRQ_IOCTL_RESET_STATS:
        spin_lock_irqsave(&g_irq_dev.lock, flags);
        g_irq_dev.interrupt_count = 0;
        g_irq_dev.last_interrupt_ns = 0;
        g_irq_dev.last_cpu = 0;
        spin_unlock_irqrestore(&g_irq_dev.lock, flags);
        pr_info("Interrupt statistics reset\n");
        return 0;

    default:
        return -ENOTTY;
    }
}

static const struct file_operations irq_demo_fops = {
    .owner          = THIS_MODULE,
    .open           = irq_demo_open,
    .release        = irq_demo_release,
    .unlocked_ioctl = irq_demo_ioctl,
};

static int __init irq_demo_init(void)
{
    int ret;

    pr_info("Initializing Interrupt Handling Driver (Chapter 13)\n");

    spin_lock_init(&g_irq_dev.lock);
    g_irq_dev.interrupt_count = 0;
    g_irq_dev.last_interrupt_ns = 0;
    g_irq_dev.last_cpu = 0;
    g_irq_dev.hw_registered = false;
    g_irq_dev.active_irq = irq_num;

    /* 1. Allocate character device region */
    ret = alloc_chrdev_region(&g_irq_dev.dev_num, 0, 1, DEVICE_NAME);
    if (ret < 0) {
        pr_err("alloc_chrdev_region failed (%d)\n", ret);
        return ret;
    }

    /* 2. Add cdev */
    cdev_init(&g_irq_dev.cdev, &irq_demo_fops);
    g_irq_dev.cdev.owner = THIS_MODULE;

    ret = cdev_add(&g_irq_dev.cdev, g_irq_dev.dev_num, 1);
    if (ret < 0) {
        pr_err("cdev_add failed (%d)\n", ret);
        goto unregister_chrdev;
    }

    /* 3. Create class */
    g_irq_dev.dev_class = class_create(CLASS_NAME);
    if (IS_ERR(g_irq_dev.dev_class)) {
        ret = PTR_ERR(g_irq_dev.dev_class);
        pr_err("class_create failed (%d)\n", ret);
        goto delete_cdev;
    }

    /* 4. Create device node */
    g_irq_dev.device = device_create(g_irq_dev.dev_class, NULL, g_irq_dev.dev_num,
                                     NULL, DEVICE_NAME);
    if (IS_ERR(g_irq_dev.device)) {
        ret = PTR_ERR(g_irq_dev.device);
        pr_err("device_create failed (%d)\n", ret);
        goto destroy_class;
    }

    /* 5. Attempt hardware IRQ line registration if requested */
    if (irq_num > 0) {
        ret = request_irq(irq_num, isr_top_half, IRQF_SHARED, DEVICE_NAME, &g_irq_dev);
        if (ret == 0) {
            g_irq_dev.hw_registered = true;
            pr_info("Successfully attached ISR to Hardware IRQ %d (IRQF_SHARED)\n", irq_num);
        } else {
            pr_warn("Could not bind hardware IRQ %d (err=%d). Falling back to software IRQ simulation.\n",
                    irq_num, ret);
        }
    } else {
        pr_info("Running in software interrupt emulation mode (irq_num=0)\n");
    }

    pr_info("Interrupt driver loaded: /dev/%s active\n", DEVICE_NAME);
    return 0;

destroy_class:
    class_destroy(g_irq_dev.dev_class);
delete_cdev:
    cdev_del(&g_irq_dev.cdev);
unregister_chrdev:
    unregister_chrdev_region(g_irq_dev.dev_num, 1);
    return ret;
}

static void __exit irq_demo_exit(void)
{
    pr_info("Unloading Interrupt Handling Driver\n");

    if (g_irq_dev.hw_registered) {
        free_irq(g_irq_dev.active_irq, &g_irq_dev);
        pr_info("Hardware IRQ %d freed\n", g_irq_dev.active_irq);
    }

    device_destroy(g_irq_dev.dev_class, g_irq_dev.dev_num);
    class_destroy(g_irq_dev.dev_class);
    cdev_del(&g_irq_dev.cdev);
    unregister_chrdev_region(g_irq_dev.dev_num, 1);

    pr_info("Driver unloaded cleanly\n");
}

module_init(irq_demo_init);
module_exit(irq_demo_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Deepak Pandey");
MODULE_DESCRIPTION("Chapter 13: Hardware & Shared Interrupt Handling (request_irq, IRQF_SHARED, Top-Half ISR)");
MODULE_VERSION("1.0");
