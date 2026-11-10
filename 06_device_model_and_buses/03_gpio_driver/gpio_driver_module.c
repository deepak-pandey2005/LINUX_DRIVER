// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file gpio_driver_module.c
 * @brief Chapter 18: GPIO Subsystem & Modern Descriptor-Based gpiod Consumer API
 * @author Deepak Pandey
 *
 * This module demonstrates:
 *  - Implementing a virtual GPIO controller using struct gpio_chip and gpiochip_add_data().
 *  - Migrating from legacy integer-based GPIO to modern descriptor-based API (struct gpio_desc).
 *  - Configuring pin directions using gpiod_direction_input() and gpiod_direction_output().
 *  - Reading and writing logical levels with gpiod_get_value() and gpiod_set_value().
 *  - Exposing GPIO control via a dedicated character device /dev/virtual_gpio.
 *  - Safe synchronization with spinlocks.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/gpio/driver.h>
#include <linux/gpio/consumer.h>
#include <linux/uaccess.h>
#include <linux/spinlock.h>

#include "gpio_ioctl.h"

#define DEVICE_NAME "virtual_gpio"
#define CLASS_NAME  "gpio_demo_class"
#define NUM_GPIOS   8

/**
 * @brief Virtual GPIO device state
 */
struct virtual_gpio_dev {
    dev_t dev_num;
    struct cdev cdev;
    struct class *dev_class;
    struct device *device;

    spinlock_t lock;
    u8 pin_values;      /* Bitmask of line values (0 or 1) */
    u8 pin_directions;  /* Bitmask: 1=IN, 0=OUT */

    struct gpio_chip chip;
    struct gpio_desc *descs[NUM_GPIOS];
};

static struct virtual_gpio_dev g_gpio_dev;

/* =========================================================================
 * struct gpio_chip Controller Callbacks
 * ========================================================================= */

static int vgpio_get_direction(struct gpio_chip *gc, unsigned int offset)
{
    struct virtual_gpio_dev *dev = gpiochip_get_data(gc);
    unsigned long flags;
    int dir;

    spin_lock_irqsave(&dev->lock, flags);
    dir = (dev->pin_directions & BIT(offset)) ? GPIO_LINE_DIRECTION_IN : GPIO_LINE_DIRECTION_OUT;
    spin_unlock_irqrestore(&dev->lock, flags);

    return dir;
}

static int vgpio_direction_input(struct gpio_chip *gc, unsigned int offset)
{
    struct virtual_gpio_dev *dev = gpiochip_get_data(gc);
    unsigned long flags;

    spin_lock_irqsave(&dev->lock, flags);
    dev->pin_directions |= BIT(offset);
    spin_unlock_irqrestore(&dev->lock, flags);

    return 0;
}

static int vgpio_direction_output(struct gpio_chip *gc, unsigned int offset, int value)
{
    struct virtual_gpio_dev *dev = gpiochip_get_data(gc);
    unsigned long flags;

    spin_lock_irqsave(&dev->lock, flags);
    dev->pin_directions &= ~BIT(offset);
    if (value)
        dev->pin_values |= BIT(offset);
    else
        dev->pin_values &= ~BIT(offset);
    spin_unlock_irqrestore(&dev->lock, flags);

    return 0;
}

static int vgpio_get(struct gpio_chip *gc, unsigned int offset)
{
    struct virtual_gpio_dev *dev = gpiochip_get_data(gc);
    unsigned long flags;
    int val;

    spin_lock_irqsave(&dev->lock, flags);
    val = (dev->pin_values & BIT(offset)) ? 1 : 0;
    spin_unlock_irqrestore(&dev->lock, flags);

    return val;
}

static int vgpio_set(struct gpio_chip *gc, unsigned int offset, int value)
{
    struct virtual_gpio_dev *dev = gpiochip_get_data(gc);
    unsigned long flags;

    spin_lock_irqsave(&dev->lock, flags);
    if (value)
        dev->pin_values |= BIT(offset);
    else
        dev->pin_values &= ~BIT(offset);
    spin_unlock_irqrestore(&dev->lock, flags);

    return 0;
}

/* =========================================================================
 * VFS File Operations & IOCTL
 * ========================================================================= */

static int gpio_demo_open(struct inode *inode, struct file *file)
{
    return 0;
}

static int gpio_demo_release(struct inode *inode, struct file *file)
{
    return 0;
}

static long gpio_demo_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    struct gpio_pin_req req;
    struct gpio_chip_info info;
    u32 pin_offset;
    unsigned long flags;

    switch (cmd) {
    case GPIO_IOCTL_SET_DIR_OUT:
        if (copy_from_user(&req, (void __user *)arg, sizeof(req)))
            return -EFAULT;
        if (req.pin_offset >= NUM_GPIOS)
            return -EINVAL;

        /* Use modern descriptor-based consumer API */
        gpiod_direction_output(g_gpio_dev.descs[req.pin_offset], req.value ? 1 : 0);
        return 0;

    case GPIO_IOCTL_SET_DIR_IN:
        if (copy_from_user(&pin_offset, (void __user *)arg, sizeof(pin_offset)))
            return -EFAULT;
        if (pin_offset >= NUM_GPIOS)
            return -EINVAL;

        gpiod_direction_input(g_gpio_dev.descs[pin_offset]);
        return 0;

    case GPIO_IOCTL_WRITE_PIN:
        if (copy_from_user(&req, (void __user *)arg, sizeof(req)))
            return -EFAULT;
        if (req.pin_offset >= NUM_GPIOS)
            return -EINVAL;

        gpiod_set_value(g_gpio_dev.descs[req.pin_offset], req.value ? 1 : 0);
        return 0;

    case GPIO_IOCTL_READ_PIN:
        if (copy_from_user(&req, (void __user *)arg, sizeof(req)))
            return -EFAULT;
        if (req.pin_offset >= NUM_GPIOS)
            return -EINVAL;

        req.value = gpiod_get_value(g_gpio_dev.descs[req.pin_offset]);
        if (copy_to_user((void __user *)arg, &req, sizeof(req)))
            return -EFAULT;
        return 0;

    case GPIO_IOCTL_GET_CHIP_INFO:
        spin_lock_irqsave(&g_gpio_dev.lock, flags);
        info.base = g_gpio_dev.chip.base;
        info.ngpio = g_gpio_dev.chip.ngpio;
        info.pin_values = g_gpio_dev.pin_values;
        info.pin_directions = g_gpio_dev.pin_directions;
        spin_unlock_irqrestore(&g_gpio_dev.lock, flags);

        if (copy_to_user((void __user *)arg, &info, sizeof(info)))
            return -EFAULT;
        return 0;

    default:
        return -ENOTTY;
    }
}

static const struct file_operations gpio_demo_fops = {
    .owner          = THIS_MODULE,
    .open           = gpio_demo_open,
    .release        = gpio_demo_release,
    .unlocked_ioctl = gpio_demo_ioctl,
};

static int __init virtual_gpio_init(void)
{
    int ret, i;

    pr_info("Initializing Chapter 18 Virtual GPIO Driver\n");

    spin_lock_init(&g_gpio_dev.lock);
    g_gpio_dev.pin_values = 0;
    g_gpio_dev.pin_directions = 0xFF; /* Default: all pins input */

    /* 1. Configure and register struct gpio_chip */
    g_gpio_dev.chip.label = "virtual_gpio_chip";
    g_gpio_dev.chip.owner = THIS_MODULE;
    g_gpio_dev.chip.base  = -1; /* Dynamic allocation */
    g_gpio_dev.chip.ngpio = NUM_GPIOS;
    g_gpio_dev.chip.can_sleep = false;
    g_gpio_dev.chip.get_direction = vgpio_get_direction;
    g_gpio_dev.chip.direction_input = vgpio_direction_input;
    g_gpio_dev.chip.direction_output = vgpio_direction_output;
    g_gpio_dev.chip.get = vgpio_get;
    g_gpio_dev.chip.set = vgpio_set;

    ret = gpiochip_add_data(&g_gpio_dev.chip, &g_gpio_dev);
    if (ret) {
        pr_err("gpiochip_add_data failed (%d)\n", ret);
        return ret;
    }

    pr_info("Virtual GPIO chip registered with base = %d (%d lines)\n",
            g_gpio_dev.chip.base, g_gpio_dev.chip.ngpio);

    /* 2. Acquire modern descriptors for all pins */
    for (i = 0; i < NUM_GPIOS; i++) {
        g_gpio_dev.descs[i] = gpio_to_desc(g_gpio_dev.chip.base + i);
        if (!g_gpio_dev.descs[i]) {
            pr_err("Failed to get descriptor for pin %d\n", i);
            ret = -ENODEV;
            goto remove_chip;
        }
    }

    /* 3. Register character device interface */
    ret = alloc_chrdev_region(&g_gpio_dev.dev_num, 0, 1, DEVICE_NAME);
    if (ret < 0)
        goto remove_chip;

    cdev_init(&g_gpio_dev.cdev, &gpio_demo_fops);
    g_gpio_dev.cdev.owner = THIS_MODULE;
    ret = cdev_add(&g_gpio_dev.cdev, g_gpio_dev.dev_num, 1);
    if (ret < 0)
        goto unregister_chrdev;

    g_gpio_dev.dev_class = class_create(CLASS_NAME);
    if (IS_ERR(g_gpio_dev.dev_class)) {
        ret = PTR_ERR(g_gpio_dev.dev_class);
        goto del_cdev;
    }

    g_gpio_dev.device = device_create(g_gpio_dev.dev_class, NULL, g_gpio_dev.dev_num,
                                      NULL, DEVICE_NAME);
    if (IS_ERR(g_gpio_dev.device)) {
        ret = PTR_ERR(g_gpio_dev.device);
        goto destroy_class;
    }

    pr_info("Virtual GPIO controller ready: /dev/%s active\n", DEVICE_NAME);
    return 0;

destroy_class:
    class_destroy(g_gpio_dev.dev_class);
del_cdev:
    cdev_del(&g_gpio_dev.cdev);
unregister_chrdev:
    unregister_chrdev_region(g_gpio_dev.dev_num, 1);
remove_chip:
    gpiochip_remove(&g_gpio_dev.chip);
    return ret;
}

static void __exit virtual_gpio_exit(void)
{
    pr_info("Unloading Virtual GPIO Driver\n");

    device_destroy(g_gpio_dev.dev_class, g_gpio_dev.dev_num);
    class_destroy(g_gpio_dev.dev_class);
    cdev_del(&g_gpio_dev.cdev);
    unregister_chrdev_region(g_gpio_dev.dev_num, 1);
    gpiochip_remove(&g_gpio_dev.chip);

    pr_info("Driver unloaded cleanly\n");
}

module_init(virtual_gpio_init);
module_exit(virtual_gpio_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Deepak Pandey");
MODULE_DESCRIPTION("Chapter 18: GPIO Subsystem & Modern Descriptor-Based gpiod Consumer API");
MODULE_VERSION("1.0");
