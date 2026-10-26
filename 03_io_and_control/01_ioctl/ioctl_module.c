// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file ioctl_module.c
 * @brief Chapter 08: IOCTL Subsystem & Custom Hardware Control Commands.
 * @author Deepak Pandey
 *
 * This module demonstrates:
 *  - Implementing the modern unlocked_ioctl() file operation.
 *  - Decoding and validating IOCTL magic numbers, command types, and direction.
 *  - Safe memory transfer between user-space and kernel-space via copy_from_user() and copy_to_user().
 *  - Mutual exclusion using mutexes to protect internal device state.
 *  - Returning POSIX standard error codes (-ENOTTY for unknown commands, -EINVAL for invalid parameters, -EFAULT for memory violations).
 *  - Automatic device node creation (/dev/ioctl_dev) with 0666 permissions.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/ktime.h>
#include <linux/ctype.h>

#include "ioctl_commands.h"

#define DEVICE_NAME "ioctl_dev"
#define CLASS_NAME  "ioctl_class"

/**
 * @brief Device private data and hardware state representation
 */
struct ioctl_dev_state {
    dev_t dev_num;
    struct cdev cdev;
    struct class *class;
    struct device *device;

    struct mutex lock;             /* Protects concurrent IOCTL state modifications */
    struct device_config config;   /* Configurable hardware parameters */
    struct device_status status;   /* Operational status and metrics */
    u64 load_time_sec;             /* Boot-relative timestamp at module load */
};

static struct ioctl_dev_state ioctl_dev;

/**
 * @brief Devnode callback to set /dev/ioctl_dev permissions to 0666
 */
static char *ioctl_devnode(const struct device *dev, umode_t *mode)
{
    if (mode) {
        *mode = 0666;
    }
    return NULL;
}

/* =========================================================================
 * VFS File Operations
 * ========================================================================= */

static int ioctl_open(struct inode *inode, struct file *file)
{
    struct ioctl_dev_state *dev = container_of(inode->i_cdev, struct ioctl_dev_state, cdev);
    file->private_data = dev;

    pr_info("Device opened by PID %d (%s)\n", current->pid, current->comm);
    return 0;
}

static int ioctl_release(struct inode *inode, struct file *file)
{
    pr_info("Device closed by PID %d (%s)\n", current->pid, current->comm);
    return 0;
}

/**
 * @brief Handle user-space ioctl() invocations
 *
 * @param file Pointer to VFS file descriptor
 * @param cmd Encoded 32-bit IOCTL command (dir, size, magic, nr)
 * @param arg User-space argument pointer / scalar integer
 * @return 0 on success, negative error code on failure (-ENOTTY, -EINVAL, -EFAULT)
 */
static long ioctl_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    struct ioctl_dev_state *dev = file->private_data;
    int ret = 0;

    /* Step 1: Validate Magic Number */
    if (_IOC_TYPE(cmd) != IOCTL_MAGIC) {
        pr_warn("Invalid magic number received: 0x%02x (expected '%c' = 0x%02x)\n",
                _IOC_TYPE(cmd), IOCTL_MAGIC, IOCTL_MAGIC);
        return -ENOTTY;
    }

    /* Step 2: Dispatch and process commands */
    switch (cmd) {
    case IOCTL_RESET_DEVICE: {
        pr_info("IOCTL_RESET_DEVICE: Resetting configuration and statistics\n");

        mutex_lock(&dev->lock);
        dev->config.baud_rate = 115200;
        dev->config.timeout_ms = 1000;
        dev->config.operation_mode = 0;
        dev->status.total_operations = 0;
        mutex_unlock(&dev->lock);
        break;
    }

    case IOCTL_SET_CONFIG: {
        struct device_config new_cfg;

        /* Safely pull payload from user space */
        if (copy_from_user(&new_cfg, (void __user *)arg, sizeof(new_cfg))) {
            pr_err("IOCTL_SET_CONFIG: copy_from_user failed\n");
            return -EFAULT;
        }

        /* Parameter validation */
        if (new_cfg.baud_rate < 1200 || new_cfg.baud_rate > 10000000) {
            pr_warn("IOCTL_SET_CONFIG: Unsupported baud rate %u\n", new_cfg.baud_rate);
            return -EINVAL;
        }
        if (new_cfg.operation_mode > 2) {
            pr_warn("IOCTL_SET_CONFIG: Invalid operation mode %u\n", new_cfg.operation_mode);
            return -EINVAL;
        }

        mutex_lock(&dev->lock);
        dev->config = new_cfg;
        dev->status.total_operations++;
        mutex_unlock(&dev->lock);

        pr_info("IOCTL_SET_CONFIG applied: baud=%u, timeout=%u ms, mode=%u\n",
                new_cfg.baud_rate, new_cfg.timeout_ms, new_cfg.operation_mode);
        break;
    }

    case IOCTL_GET_STATUS: {
        struct device_status st;

        mutex_lock(&dev->lock);
        st.total_operations = dev->status.total_operations;
        st.uptime_seconds = (uint32_t)(ktime_get_boottime_seconds() - dev->load_time_sec);
        st.temperature_mC = 37000 + (int32_t)(st.total_operations * 150);
        st.device_ready = 1;
        mutex_unlock(&dev->lock);

        /* Safely push status structure to user space */
        if (copy_to_user((void __user *)arg, &st, sizeof(st))) {
            pr_err("IOCTL_GET_STATUS: copy_to_user failed\n");
            return -EFAULT;
        }

        pr_info("IOCTL_GET_STATUS sent: ops=%u, uptime=%u s, temp=%d mC\n",
                st.total_operations, st.uptime_seconds, st.temperature_mC);
        break;
    }

    case IOCTL_TRANSFORM_DATA: {
        struct exchange_payload payload;
        size_t i;
        uint32_t chk = 0;

        /* Pull data from user space */
        if (copy_from_user(&payload, (void __user *)arg, sizeof(payload))) {
            pr_err("IOCTL_TRANSFORM_DATA: copy_from_user failed\n");
            return -EFAULT;
        }

        /* Enforce null-termination safety */
        payload.message[sizeof(payload.message) - 1] = '\0';

        /* Transform: convert to uppercase and calculate simple sum checksum */
        for (i = 0; payload.message[i] != '\0'; i++) {
            payload.message[i] = (char)toupper((unsigned char)payload.message[i]);
            chk = (chk * 31) + (uint8_t)payload.message[i];
        }
        payload.checksum = chk;

        mutex_lock(&dev->lock);
        dev->status.total_operations++;
        mutex_unlock(&dev->lock);

        /* Push transformed data and calculated checksum back to user space */
        if (copy_to_user((void __user *)arg, &payload, sizeof(payload))) {
            pr_err("IOCTL_TRANSFORM_DATA: copy_to_user failed\n");
            return -EFAULT;
        }

        pr_info("IOCTL_TRANSFORM_DATA: transformed msg='%s', chk=0x%08X\n",
                payload.message, payload.checksum);
        break;
    }

    default:
        pr_warn("Unrecognized IOCTL command: 0x%08x (nr=%u, dir=%u, size=%u)\n",
                cmd, _IOC_NR(cmd), _IOC_DIR(cmd), _IOC_SIZE(cmd));
        ret = -ENOTTY;
        break;
    }

    return ret;
}

static const struct file_operations ioctl_fops = {
    .owner          = THIS_MODULE,
    .open           = ioctl_open,
    .release        = ioctl_release,
    .unlocked_ioctl = ioctl_unlocked_ioctl,
};

/* =========================================================================
 * Module Init / Exit
 * ========================================================================= */

static int __init ioctl_module_init(void)
{
    int ret;

    pr_info("Initializing IOCTL Subsystem module\n");

    /* Initialize device private state */
    mutex_init(&ioctl_dev.lock);
    ioctl_dev.config.baud_rate = 115200;
    ioctl_dev.config.timeout_ms = 1000;
    ioctl_dev.config.operation_mode = 0;
    ioctl_dev.status.total_operations = 0;
    ioctl_dev.status.device_ready = 1;
    ioctl_dev.load_time_sec = ktime_get_boottime_seconds();

    /* Step 1: Allocate dynamic major/minor number */
    ret = alloc_chrdev_region(&ioctl_dev.dev_num, 0, 1, DEVICE_NAME);
    if (ret < 0) {
        pr_err("alloc_chrdev_region failed: %d\n", ret);
        return ret;
    }
    pr_info("Allocated dev_num: Major=%d, Minor=%d\n",
            MAJOR(ioctl_dev.dev_num), MINOR(ioctl_dev.dev_num));

    /* Step 2: Initialize and register cdev */
    cdev_init(&ioctl_dev.cdev, &ioctl_fops);
    ioctl_dev.cdev.owner = THIS_MODULE;

    ret = cdev_add(&ioctl_dev.cdev, ioctl_dev.dev_num, 1);
    if (ret < 0) {
        pr_err("cdev_add failed: %d\n", ret);
        goto fail_cdev;
    }

    /* Step 3: Create sysfs class */
    ioctl_dev.class = class_create(CLASS_NAME);
    if (IS_ERR(ioctl_dev.class)) {
        ret = PTR_ERR(ioctl_dev.class);
        pr_err("class_create failed: %d\n", ret);
        goto fail_class;
    }
    ioctl_dev.class->devnode = ioctl_devnode;

    /* Step 4: Create device node */
    ioctl_dev.device = device_create(ioctl_dev.class, NULL, ioctl_dev.dev_num,
                                    NULL, DEVICE_NAME);
    if (IS_ERR(ioctl_dev.device)) {
        ret = PTR_ERR(ioctl_dev.device);
        pr_err("device_create failed: %d\n", ret);
        goto fail_device;
    }

    pr_info("Device /dev/%s registered successfully!\n", DEVICE_NAME);
    return 0;

fail_device:
    class_destroy(ioctl_dev.class);
fail_class:
    cdev_del(&ioctl_dev.cdev);
fail_cdev:
    unregister_chrdev_region(ioctl_dev.dev_num, 1);
    return ret;
}

static void __exit ioctl_module_exit(void)
{
    pr_info("Cleaning up IOCTL Subsystem module\n");

    device_destroy(ioctl_dev.class, ioctl_dev.dev_num);
    class_destroy(ioctl_dev.class);
    cdev_del(&ioctl_dev.cdev);
    unregister_chrdev_region(ioctl_dev.dev_num, 1);
    mutex_destroy(&ioctl_dev.lock);

    pr_info("Cleanup complete. Device unregistered.\n");
}

module_init(ioctl_module_init);
module_exit(ioctl_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Deepak Pandey");
MODULE_DESCRIPTION("Chapter 08: Production IOCTL Subsystem & Custom Hardware Control");
MODULE_VERSION("1.0");
