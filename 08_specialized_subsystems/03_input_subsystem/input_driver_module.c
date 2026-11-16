// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file input_driver_module.c
 * @brief Chapter 26: Linux Input Subsystem & Virtual Controller Driver
 * @author Deepak Pandey
 *
 * This module demonstrates:
 *  - Architecture of the Linux Input Subsystem (input_dev, event handlers, evdev).
 *  - Allocating and configuring input devices using input_allocate_device().
 *  - Declaring capabilities: EV_KEY (gamepad buttons, arrow keys) and EV_ABS (analog thumbsticks).
 *  - Generating input events at runtime: input_report_key(), input_report_abs(), input_sync().
 *  - Exposing control character device (/dev/vcontroller) to inject synthesized input events.
 *  - Interfacing with the standard /dev/input/eventX device nodes and evtest utility.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/input.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/device.h>

#define DRIVER_NAME   "vcontroller_driver"
#define DEV_NAME      "vcontroller"
#define BUFFER_SIZE   128

struct vinput_priv {
    struct input_dev *idev;
    dev_t dev_num;
    struct cdev cdev;
    struct class *class;
    struct device *device;
    struct mutex lock;

    /* Telemetry statistics */
    u64 total_events_injected;
    u64 total_key_events;
    u64 total_abs_events;
    int last_event_code;
    int last_event_value;
};

static struct vinput_priv *g_vinput;

/**
 * @brief Character device write operation: parses command and injects input event
 *
 * Supported command formats:
 *  - "KEY_A 1" / "KEY_A 0"     -> BTN_A press / release
 *  - "KEY_B 1" / "KEY_B 0"     -> BTN_B press / release
 *  - "KEY_UP 1" / "KEY_UP 0"   -> KEY_UP press / release
 *  - "KEY_DOWN 1"              -> KEY_DOWN press
 *  - "ABS_X <int>"             -> Analog X axis position (-512 to 512)
 *  - "ABS_Y <int>"             -> Analog Y axis position (-512 to 512)
 */
static ssize_t vcontroller_write(struct file *file, const char __user *user_buf,
                                 size_t count, loff_t *ppos)
{
    struct vinput_priv *priv = file->private_data;
    char kbuf[BUFFER_SIZE];
    char cmd[32];
    int val = 0;
    size_t to_copy;

    if (!priv || !priv->idev)
        return -ENODEV;

    to_copy = min(count, (size_t)(sizeof(kbuf) - 1));

    if (copy_from_user(kbuf, user_buf, to_copy))
        return -EFAULT;

    kbuf[to_copy] = '\0';

    if (sscanf(kbuf, "%31s %d", cmd, &val) != 2) {
        pr_warn("Invalid format. Expected '<CMD> <VALUE>', received: %s\n", kbuf);
        return -EINVAL;
    }

    if (mutex_lock_interruptible(&priv->lock))
        return -ERESTARTSYS;

    if (strcmp(cmd, "KEY_A") == 0) {
        input_report_key(priv->idev, BTN_A, !!val);
        input_sync(priv->idev);
        priv->total_key_events++;
        priv->last_event_code = BTN_A;
    } else if (strcmp(cmd, "KEY_B") == 0) {
        input_report_key(priv->idev, BTN_B, !!val);
        input_sync(priv->idev);
        priv->total_key_events++;
        priv->last_event_code = BTN_B;
    } else if (strcmp(cmd, "KEY_UP") == 0) {
        input_report_key(priv->idev, KEY_UP, !!val);
        input_sync(priv->idev);
        priv->total_key_events++;
        priv->last_event_code = KEY_UP;
    } else if (strcmp(cmd, "KEY_DOWN") == 0) {
        input_report_key(priv->idev, KEY_DOWN, !!val);
        input_sync(priv->idev);
        priv->total_key_events++;
        priv->last_event_code = KEY_DOWN;
    } else if (strcmp(cmd, "ABS_X") == 0) {
        val = clamp(val, -512, 512);
        input_report_abs(priv->idev, ABS_X, val);
        input_sync(priv->idev);
        priv->total_abs_events++;
        priv->last_event_code = ABS_X;
    } else if (strcmp(cmd, "ABS_Y") == 0) {
        val = clamp(val, -512, 512);
        input_report_abs(priv->idev, ABS_Y, val);
        input_sync(priv->idev);
        priv->total_abs_events++;
        priv->last_event_code = ABS_Y;
    } else {
        mutex_unlock(&priv->lock);
        pr_warn("Unrecognized input command: %s\n", cmd);
        return -EINVAL;
    }

    priv->total_events_injected++;
    priv->last_event_value = val;
    mutex_unlock(&priv->lock);

    pr_info("Injected event %s=%d into input core\n", cmd, val);
    return count;
}

static int vcontroller_open(struct inode *inode, struct file *file)
{
    file->private_data = g_vinput;
    return 0;
}

static int vcontroller_release(struct inode *inode, struct file *file)
{
    return 0;
}

static const struct file_operations vcontroller_fops = {
    .owner   = THIS_MODULE,
    .open    = vcontroller_open,
    .release = vcontroller_release,
    .write   = vcontroller_write,
};

/* Sysfs Statistics Attribute */
static ssize_t input_stats_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct vinput_priv *priv = g_vinput;

    if (!priv)
        return -ENODEV;

    return sysfs_emit(buf,
                      "Device Name:          %s\n"
                      "Total Events:         %llu\n"
                      "Key Events:           %llu\n"
                      "Abs Events:           %llu\n"
                      "Last Event Code:      0x%04x (%d)\n"
                      "Last Event Value:     %d\n",
                      priv->idev->name,
                      priv->total_events_injected,
                      priv->total_key_events,
                      priv->total_abs_events,
                      priv->last_event_code, priv->last_event_code,
                      priv->last_event_value);
}
static DEVICE_ATTR_RO(input_stats);

static int __init vinput_init(void)
{
    int ret;
    struct vinput_priv *priv;

    pr_info("Initializing Chapter 26 Virtual Input Subsystem Module\n");

    priv = kzalloc(sizeof(*priv), GFP_KERNEL);
    if (!priv)
        return -ENOMEM;

    mutex_init(&priv->lock);

    /* 1. Allocate input device */
    priv->idev = input_allocate_device();
    if (!priv->idev) {
        ret = -ENOMEM;
        goto err_kfree;
    }

    priv->idev->name       = "Virtual Gamepad Controller";
    priv->idev->phys       = "vcontroller/input0";
    priv->idev->id.bustype = BUS_VIRTUAL;
    priv->idev->id.vendor  = 0xDEAD;
    priv->idev->id.product = 0xBEEF;
    priv->idev->id.version = 0x0100;

    /* 2. Configure Capabilities (EV_KEY and EV_ABS) */
    set_bit(EV_KEY, priv->idev->evbit);
    set_bit(BTN_A, priv->idev->keybit);
    set_bit(BTN_B, priv->idev->keybit);
    set_bit(KEY_UP, priv->idev->keybit);
    set_bit(KEY_DOWN, priv->idev->keybit);

    set_bit(EV_ABS, priv->idev->evbit);
    input_set_abs_params(priv->idev, ABS_X, -512, 512, 0, 0);
    input_set_abs_params(priv->idev, ABS_Y, -512, 512, 0, 0);

    /* 3. Register input device with Linux Input Core */
    ret = input_register_device(priv->idev);
    if (ret) {
        pr_err("input_register_device failed (%d)\n", ret);
        input_free_device(priv->idev);
        goto err_kfree;
    }

    /* 4. Register control character device */
    ret = alloc_chrdev_region(&priv->dev_num, 0, 1, DEV_NAME);
    if (ret < 0) {
        pr_err("alloc_chrdev_region failed (%d)\n", ret);
        goto err_input_unreg;
    }

    cdev_init(&priv->cdev, &vcontroller_fops);
    priv->cdev.owner = THIS_MODULE;

    ret = cdev_add(&priv->cdev, priv->dev_num, 1);
    if (ret < 0) {
        pr_err("cdev_add failed (%d)\n", ret);
        goto err_chrdev;
    }

    priv->class = class_create("vcontroller_class");
    if (IS_ERR(priv->class)) {
        ret = PTR_ERR(priv->class);
        goto err_cdev;
    }

    priv->device = device_create(priv->class, NULL, priv->dev_num, NULL, DEV_NAME);
    if (IS_ERR(priv->device)) {
        ret = PTR_ERR(priv->device);
        goto err_class;
    }

    ret = device_create_file(priv->device, &dev_attr_input_stats);
    if (ret)
        pr_warn("device_create_file failed (%d)\n", ret);

    g_vinput = priv;
    pr_info("Virtual input controller registered! Control node: /dev/%s\n", DEV_NAME);
    return 0;

err_class:
    class_destroy(priv->class);
err_cdev:
    cdev_del(&priv->cdev);
err_chrdev:
    unregister_chrdev_region(priv->dev_num, 1);
err_input_unreg:
    input_unregister_device(priv->idev);
err_kfree:
    kfree(priv);
    return ret;
}

static void __exit vinput_exit(void)
{
    pr_info("Unloading Chapter 26 Virtual Input Subsystem Module\n");

    if (g_vinput) {
        device_remove_file(g_vinput->device, &dev_attr_input_stats);
        device_destroy(g_vinput->class, g_vinput->dev_num);
        class_destroy(g_vinput->class);
        cdev_del(&g_vinput->cdev);
        unregister_chrdev_region(g_vinput->dev_num, 1);

        input_unregister_device(g_vinput->idev);
        kfree(g_vinput);
        g_vinput = NULL;
    }

    pr_info("Input device unregistered and cleaned up\n");
}

module_init(vinput_init);
module_exit(vinput_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Deepak Pandey");
MODULE_DESCRIPTION("Chapter 26: Linux Input Subsystem & Virtual Controller Driver");
MODULE_VERSION("1.0");
