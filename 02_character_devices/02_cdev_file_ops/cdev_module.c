// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file cdev_module.c
 * @brief Chapter 05: Character Device Registration with struct cdev and file_operations.
 * @author Deepak Pandey
 *
 * This module demonstrates:
 *  - Initializing and registering `struct cdev` with `cdev_init()` and `cdev_add()`.
 *  - Defining `struct file_operations` for `open`, `release`, `read`, and `write`.
 *  - Utilizing `file->private_data` to track per-open state.
 *  - Safe kernel-user memory transfers using `copy_to_user()` and `copy_from_user()`.
 *  - Managing file read/write offsets (`*f_pos`).
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h> // copy_to_user, copy_from_user

#define DEVICE_NAME "cdev_buffer_dev"
#define MEM_BUFFER_SIZE 1024

// Device state container
struct custom_device_data {
    dev_t dev_num;
    struct cdev cdev;
    char buffer[MEM_BUFFER_SIZE];
    size_t data_size;
    int open_count;
};

static struct custom_device_data dev_data;

/* =========================================================================
 * VFS File Operations Callbacks
 * ========================================================================= */

/**
 * @brief Called when a user-space process opens the device node.
 */
static int dev_open(struct inode *inode, struct file *file)
{
    // Retrieve our device data from inode's cdev pointer
    struct custom_device_data *data = container_of(inode->i_cdev, struct custom_device_data, cdev);

    // Stash it in file->private_data for read/write calls
    file->private_data = data;
    data->open_count++;

    pr_info("Device opened! Active open handles: %d (PID: %d, Comm: %s)\n",
            data->open_count, current->pid, current->comm);

    return 0;
}

/**
 * @brief Called when a user-space process closes the file descriptor.
 */
static int dev_release(struct inode *inode, struct file *file)
{
    struct custom_device_data *data = file->private_data;

    if (data) {
        data->open_count--;
        pr_info("Device closed! Remaining handles: %d\n", data->open_count);
    }

    return 0;
}

/**
 * @brief Called when user space reads from the device file descriptor.
 */
static ssize_t dev_read(struct file *file, char __user *user_buf, size_t count, loff_t *f_pos)
{
    struct custom_device_data *data = file->private_data;
    size_t bytes_to_copy;
    size_t not_copied;

    if (!data) {
        return -EFAULT;
    }

    // Check if offset is at or beyond the current stored data size (EOF)
    if (*f_pos >= data->data_size) {
        return 0; // EOF (End of File)
    }

    // Limit bytes to what is remaining in our buffer
    bytes_to_copy = min(count, (size_t)(data->data_size - *f_pos));

    // Safely copy data from kernel buffer to user-space pointer
    not_copied = copy_to_user(user_buf, data->buffer + *f_pos, bytes_to_copy);
    if (not_copied != 0) {
        pr_err("copy_to_user failed for %zu bytes\n", not_copied);
        return -EFAULT;
    }

    // Advance file position offset
    *f_pos += bytes_to_copy;

    pr_info("Read %zu bytes (new offset: %lld)\n", bytes_to_copy, *f_pos);
    return bytes_to_copy;
}

/**
 * @brief Called when user space writes to the device file descriptor.
 */
static ssize_t dev_write(struct file *file, const char __user *user_buf, size_t count, loff_t *f_pos)
{
    struct custom_device_data *data = file->private_data;
    size_t bytes_to_copy;
    size_t not_copied;

    if (!data) {
        return -EFAULT;
    }

    // If offset is beyond buffer capacity, return No Space Left on Device
    if (*f_pos >= MEM_BUFFER_SIZE) {
        return -ENOSPC;
    }

    // Limit bytes to available room in buffer
    bytes_to_copy = min(count, (size_t)(MEM_BUFFER_SIZE - *f_pos));

    // Safely copy from user space into kernel buffer
    not_copied = copy_from_user(data->buffer + *f_pos, user_buf, bytes_to_copy);
    if (not_copied != 0) {
        pr_err("copy_from_user failed for %zu bytes\n", not_copied);
        return -EFAULT;
    }

    *f_pos += bytes_to_copy;
    data->data_size = *f_pos; // Update current payload length

    pr_info("Wrote %zu bytes into kernel buffer (total stored: %zu)\n",
            bytes_to_copy, data->data_size);

    return bytes_to_copy;
}

// File operations structure mapping VFS to our functions
static const struct file_operations fops = {
    .owner   = THIS_MODULE,
    .open    = dev_open,
    .release = dev_release,
    .read    = dev_read,
    .write   = dev_write,
};

/* =========================================================================
 * Module Lifecycle
 * ========================================================================= */

static int __init cdev_example_init(void)
{
    int ret;

    pr_info("==================================================\n");
    pr_info("Initializing struct cdev & file_operations lab...\n");

    // 1. Allocate device number
    ret = alloc_chrdev_region(&dev_data.dev_num, 0, 1, DEVICE_NAME);
    if (ret < 0) {
        pr_err("alloc_chrdev_region failed: %d\n", ret);
        return ret;
    }

    pr_info("Allocated Major: %d, Minor: %d\n",
            MAJOR(dev_data.dev_num), MINOR(dev_data.dev_num));

    // 2. Initialize the struct cdev with our file operations
    cdev_init(&dev_data.cdev, &fops);
    dev_data.cdev.owner = THIS_MODULE;

    // 3. Register the cdev with the kernel VFS
    // CRITICAL: Once cdev_add() succeeds, the driver is LIVE and can receive open calls!
    ret = cdev_add(&dev_data.cdev, dev_data.dev_num, 1);
    if (ret < 0) {
        pr_err("cdev_add failed: %d\n", ret);
        unregister_chrdev_region(dev_data.dev_num, 1);
        return ret;
    }

    // Initialize buffer state
    dev_data.data_size = 0;
    dev_data.open_count = 0;
    memset(dev_data.buffer, 0, MEM_BUFFER_SIZE);

    pr_info("cdev_add registered successfully!\n");
    pr_info("To create the device node run:\n");
    pr_info("  sudo mknod /dev/%s c %d 0\n", DEVICE_NAME, MAJOR(dev_data.dev_num));
    pr_info("  sudo chmod 666 /dev/%s\n", DEVICE_NAME);
    pr_info("==================================================\n");

    return 0;
}

static void __exit cdev_example_exit(void)
{
    pr_info("Cleaning up cdev module...\n");

    // 1. Delete cdev from VFS (no new open calls accepted)
    cdev_del(&dev_data.cdev);

    // 2. Release device numbers
    unregister_chrdev_region(dev_data.dev_num, 1);

    pr_info("Device unregistered cleanly.\n");
}

module_init(cdev_example_init);
module_exit(cdev_example_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Deepak Pandey");
MODULE_DESCRIPTION("Chapter 05: Character Device struct cdev & file_operations");
MODULE_VERSION("1.0.0");
