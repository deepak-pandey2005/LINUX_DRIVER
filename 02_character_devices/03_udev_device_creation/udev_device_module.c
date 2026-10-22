// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file udev_device_module.c
 * @brief Chapter 06: Dynamic Device Node Creation using class_create and udev.
 * @author Deepak Pandey
 *
 * This module demonstrates:
 *  - Automatically creating /dev/<device_name> without manual `mknod`.
 *  - Creating device classes in sysfs using `class_create()`.
 *  - Emitting uevents to `udevd` using `device_create()`.
 *  - Setting default device node permissions via `class->devnode`.
 *  - Proper error recovery using the canonical Linux kernel `goto` rollback pattern.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>   // class_create, device_create
#include <linux/uaccess.h>  // copy_to_user, copy_from_user

#define CLASS_NAME  "auto_char_class"
#define DEVICE_NAME "auto_char_dev"
#define BUFFER_SIZE 1024

// Device driver state
struct auto_device_data {
    dev_t dev_num;
    struct cdev cdev;
    struct class *class;
    struct device *device;
    char buffer[BUFFER_SIZE];
    size_t data_len;
};

static struct auto_device_data my_dev;

/* =========================================================================
 * Custom Class Devnode Callback
 * ========================================================================= */

/**
 * @brief Sets default filesystem permissions for the auto-created /dev node.
 * Returning mode 0666 allows non-root user applications to read/write.
 */
static char *auto_devnode(const struct device *dev, umode_t *mode)
{
    if (mode) {
        *mode = 0666; // rw-rw-rw-
    }
    return NULL;
}

/* =========================================================================
 * VFS File Operations
 * ========================================================================= */

static int dev_open(struct inode *inode, struct file *file)
{
    file->private_data = &my_dev;
    pr_info("Opened /dev/%s by PID %d (%s)\n", DEVICE_NAME, current->pid, current->comm);
    return 0;
}

static int dev_release(struct inode *inode, struct file *file)
{
    pr_info("Closed /dev/%s\n", DEVICE_NAME);
    return 0;
}

static ssize_t dev_read(struct file *file, char __user *user_buf, size_t count, loff_t *f_pos)
{
    struct auto_device_data *data = file->private_data;
    size_t bytes_to_copy;
    size_t not_copied;

    if (*f_pos >= data->data_len) {
        return 0; // EOF
    }

    bytes_to_copy = min(count, (size_t)(data->data_len - *f_pos));
    not_copied = copy_to_user(user_buf, data->buffer + *f_pos, bytes_to_copy);
    if (not_copied) {
        return -EFAULT;
    }

    *f_pos += bytes_to_copy;
    pr_info("Read %zu bytes from auto device\n", bytes_to_copy);
    return bytes_to_copy;
}

static ssize_t dev_write(struct file *file, const char __user *user_buf, size_t count, loff_t *f_pos)
{
    struct auto_device_data *data = file->private_data;
    size_t bytes_to_copy;
    size_t not_copied;

    if (*f_pos >= BUFFER_SIZE) {
        return -ENOSPC;
    }

    bytes_to_copy = min(count, (size_t)(BUFFER_SIZE - *f_pos));
    not_copied = copy_from_user(data->buffer + *f_pos, user_buf, bytes_to_copy);
    if (not_copied) {
        return -EFAULT;
    }

    *f_pos += bytes_to_copy;
    data->data_len = *f_pos;

    pr_info("Wrote %zu bytes to auto device (total: %zu)\n", bytes_to_copy, data->data_len);
    return bytes_to_copy;
}

static const struct file_operations fops = {
    .owner   = THIS_MODULE,
    .open    = dev_open,
    .release = dev_release,
    .read    = dev_read,
    .write   = dev_write,
};

/* =========================================================================
 * Module Lifecycle with Canonical Error Rollback
 * ========================================================================= */

static int __init auto_dev_init(void)
{
    int ret;

    pr_info("==================================================\n");
    pr_info("Initializing Dynamic Device Node (udev) Driver...\n");

    // 1. Allocate device number
    ret = alloc_chrdev_region(&my_dev.dev_num, 0, 1, DEVICE_NAME);
    if (ret < 0) {
        pr_err("Failed to allocate chrdev region: %d\n", ret);
        return ret;
    }

    // 2. Initialize and add cdev
    cdev_init(&my_dev.cdev, &fops);
    my_dev.cdev.owner = THIS_MODULE;

    ret = cdev_add(&my_dev.cdev, my_dev.dev_num, 1);
    if (ret < 0) {
        pr_err("Failed to add cdev: %d\n", ret);
        goto err_unregister_chrdev;
    }

    // 3. Create device class in /sys/class/
    my_dev.class = class_create(CLASS_NAME);
    if (IS_ERR(my_dev.class)) {
        ret = PTR_ERR(my_dev.class);
        pr_err("Failed to create device class: %d\n", ret);
        goto err_cdev_del;
    }

    // Set callback to grant 0666 permissions to created node
    my_dev.class->devnode = auto_devnode;

    // 4. Create device node in /dev (emits netlink uevent to udevd)
    my_dev.device = device_create(my_dev.class, NULL, my_dev.dev_num, NULL, DEVICE_NAME);
    if (IS_ERR(my_dev.device)) {
        ret = PTR_ERR(my_dev.device);
        pr_err("Failed to create device node: %d\n", ret);
        goto err_class_destroy;
    }

    my_dev.data_len = 0;
    memset(my_dev.buffer, 0, BUFFER_SIZE);

    pr_info("SUCCESS! Device automatically created at /dev/%s\n", DEVICE_NAME);
    pr_info("Major: %d, Minor: %d, Sysfs Class: /sys/class/%s/\n",
            MAJOR(my_dev.dev_num), MINOR(my_dev.dev_num), CLASS_NAME);
    pr_info("==================================================\n");

    return 0;

err_class_destroy:
    class_destroy(my_dev.class);
err_cdev_del:
    cdev_del(&my_dev.cdev);
err_unregister_chrdev:
    unregister_chrdev_region(my_dev.dev_num, 1);
    return ret;
}

static void __exit auto_dev_exit(void)
{
    pr_info("Cleaning up dynamic device driver...\n");

    // Clean up in exact reverse order of creation
    device_destroy(my_dev.class, my_dev.dev_num);
    class_destroy(my_dev.class);
    cdev_del(&my_dev.cdev);
    unregister_chrdev_region(my_dev.dev_num, 1);

    pr_info("/dev/%s and /sys/class/%s cleanly removed.\n", DEVICE_NAME, CLASS_NAME);
}

module_init(auto_dev_init);
module_exit(auto_dev_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Deepak Pandey");
MODULE_DESCRIPTION("Chapter 06: Dynamic Device Node Creation with class_create & udev");
MODULE_VERSION("1.0.0");
