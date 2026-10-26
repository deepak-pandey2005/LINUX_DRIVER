// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file ring_buffer_module.c
 * @brief Chapter 07: Safe Memory Transfer & Kernel Circular Ring Buffer.
 * @author Deepak Pandey
 *
 * This module demonstrates:
 *  - Implementing a circular FIFO ring buffer in kernel space.
 *  - Two-segment wrapped memory copying with `copy_to_user()` and `copy_from_user()`.
 *  - Proper boundary checks, EOF handling, and return error codes (-EFAULT, -ENOSPC).
 *  - Handling invalid user-space pointer injections safely.
 *  - Automatic device node creation via class_create & device_create.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h> // copy_to_user, copy_from_user, access_ok

#define DEVICE_NAME "ring_buffer_dev"
#define CLASS_NAME  "ring_buffer_class"
#define RING_CAPACITY 1024 // Must be a power of 2 for fast bitwise masking

/**
 * @brief Circular Ring Buffer State
 */
struct ring_buffer_dev {
    dev_t dev_num;
    struct cdev cdev;
    struct class *class;
    struct device *device;

    // FIFO State
    char buffer[RING_CAPACITY];
    size_t head;       // Write index
    size_t tail;       // Read index
    size_t count;      // Number of bytes currently queued in FIFO
};

static struct ring_buffer_dev ring_dev;

/* Set permissions of /dev/ring_buffer_dev to 0666 */
static char *ring_devnode(const struct device *dev, umode_t *mode)
{
    if (mode) {
        *mode = 0666;
    }
    return NULL;
}

/* =========================================================================
 * VFS File Operations
 * ========================================================================= */

static int ring_open(struct inode *inode, struct file *file)
{
    file->private_data = &ring_dev;
    pr_info("Device opened by PID %d (%s). Available bytes in buffer: %zu\n",
            current->pid, current->comm, ring_dev.count);
    return 0;
}

static int ring_release(struct inode *inode, struct file *file)
{
    pr_info("Device closed by PID %d (%s)\n", current->pid, current->comm);
    return 0;
}

/**
 * @brief Reads data out of the circular buffer and copies it to user space.
 * Handles wrap-around if queued data wraps past the end of the array.
 */
static ssize_t ring_read(struct file *file, char __user *user_buf, size_t count, loff_t *f_pos)
{
    struct ring_buffer_dev *dev = file->private_data;
    size_t bytes_to_read;
    size_t part1, part2;
    size_t not_copied;

    if (!user_buf) {
        return -EFAULT;
    }

    // If buffer is empty, return 0 (End of File / nothing to read)
    if (dev->count == 0) {
        pr_info("Buffer empty; returning 0 (EOF)\n");
        return 0;
    }

    // Limit read count to available queued bytes
    bytes_to_read = min(count, dev->count);

    // Calculate how many bytes can be read sequentially before wrapping
    part1 = min(bytes_to_read, (size_t)(RING_CAPACITY - dev->tail));
    part2 = bytes_to_read - part1;

    // First segment: from tail to end of array (or up to bytes_to_read)
    not_copied = copy_to_user(user_buf, dev->buffer + dev->tail, part1);
    if (not_copied) {
        pr_err("copy_to_user failed in segment 1 (failed %zu bytes)\n", not_copied);
        return -EFAULT;
    }

    // Second segment: wrapped around to index 0 (if part2 > 0)
    if (part2 > 0) {
        not_copied = copy_to_user(user_buf + part1, dev->buffer, part2);
        if (not_copied) {
            pr_err("copy_to_user failed in wrapped segment 2 (failed %zu bytes)\n", not_copied);
            return -EFAULT;
        }
    }

    // Advance tail pointer and decrease available count
    dev->tail = (dev->tail + bytes_to_read) % RING_CAPACITY;
    dev->count -= bytes_to_read;

    pr_info("Read %zu bytes (part1=%zu, part2=%zu). Remaining in buffer: %zu\n",
            bytes_to_read, part1, part2, dev->count);

    return bytes_to_read;
}

/**
 * @brief Writes data from user space into the circular buffer.
 * Handles wrap-around if new data wraps past the end of the array.
 */
static ssize_t ring_write(struct file *file, const char __user *user_buf, size_t count, loff_t *f_pos)
{
    struct ring_buffer_dev *dev = file->private_data;
    size_t available_space;
    size_t bytes_to_write;
    size_t part1, part2;
    size_t not_copied;

    if (!user_buf) {
        return -EFAULT;
    }

    // Compute free room in buffer
    available_space = RING_CAPACITY - dev->count;
    if (available_space == 0) {
        pr_warn("Buffer FULL! Cannot write more data.\n");
        return -ENOSPC; // No space left on device
    }

    // Clamp write to available space
    bytes_to_write = min(count, available_space);

    // Calculate how many bytes can be written sequentially before wrapping
    part1 = min(bytes_to_write, (size_t)(RING_CAPACITY - dev->head));
    part2 = bytes_to_write - part1;

    // First segment: from head to end of array (or up to bytes_to_write)
    not_copied = copy_from_user(dev->buffer + dev->head, user_buf, part1);
    if (not_copied) {
        pr_err("copy_from_user failed in segment 1 (failed %zu bytes)\n", not_copied);
        return -EFAULT;
    }

    // Second segment: wrapped around to index 0 (if part2 > 0)
    if (part2 > 0) {
        not_copied = copy_from_user(dev->buffer, user_buf + part1, part2);
        if (not_copied) {
            pr_err("copy_from_user failed in wrapped segment 2 (failed %zu bytes)\n", not_copied);
            return -EFAULT;
        }
    }

    // Advance head pointer and increment count
    dev->head = (dev->head + bytes_to_write) % RING_CAPACITY;
    dev->count += bytes_to_write;

    pr_info("Wrote %zu bytes (part1=%zu, part2=%zu). Total queued: %zu/%d\n",
            bytes_to_write, part1, part2, dev->count, RING_CAPACITY);

    return bytes_to_write;
}

static const struct file_operations ring_fops = {
    .owner   = THIS_MODULE,
    .open    = ring_open,
    .release = ring_release,
    .read    = ring_read,
    .write   = ring_write,
};

/* =========================================================================
 * Module Lifecycle
 * ========================================================================= */

static int __init ring_module_init(void)
{
    int ret;

    pr_info("==================================================\n");
    pr_info("Initializing Circular Ring Buffer Driver...\n");

    // Initialize circular buffer state
    ring_dev.head = 0;
    ring_dev.tail = 0;
    ring_dev.count = 0;
    memset(ring_dev.buffer, 0, RING_CAPACITY);

    // 1. Allocate device numbers
    ret = alloc_chrdev_region(&ring_dev.dev_num, 0, 1, DEVICE_NAME);
    if (ret < 0) {
        pr_err("alloc_chrdev_region failed: %d\n", ret);
        return ret;
    }

    // 2. Initialize and add cdev
    cdev_init(&ring_dev.cdev, &ring_fops);
    ring_dev.cdev.owner = THIS_MODULE;

    ret = cdev_add(&ring_dev.cdev, ring_dev.dev_num, 1);
    if (ret < 0) {
        pr_err("cdev_add failed: %d\n", ret);
        goto err_unregister_chrdev;
    }

    // 3. Create sysfs class
    ring_dev.class = class_create(CLASS_NAME);
    if (IS_ERR(ring_dev.class)) {
        ret = PTR_ERR(ring_dev.class);
        pr_err("class_create failed: %d\n", ret);
        goto err_cdev_del;
    }
    ring_dev.class->devnode = ring_devnode;

    // 4. Create device node
    ring_dev.device = device_create(ring_dev.class, NULL, ring_dev.dev_num, NULL, DEVICE_NAME);
    if (IS_ERR(ring_dev.device)) {
        ret = PTR_ERR(ring_dev.device);
        pr_err("device_create failed: %d\n", ret);
        goto err_class_destroy;
    }

    pr_info("SUCCESS: /dev/%s active with %d-byte ring buffer!\n",
            DEVICE_NAME, RING_CAPACITY);
    pr_info("==================================================\n");

    return 0;

err_class_destroy:
    class_destroy(ring_dev.class);
err_cdev_del:
    cdev_del(&ring_dev.cdev);
err_unregister_chrdev:
    unregister_chrdev_region(ring_dev.dev_num, 1);
    return ret;
}

static void __exit ring_module_exit(void)
{
    pr_info("Cleaning up Circular Ring Buffer Driver...\n");

    device_destroy(ring_dev.class, ring_dev.dev_num);
    class_destroy(ring_dev.class);
    cdev_del(&ring_dev.cdev);
    unregister_chrdev_region(ring_dev.dev_num, 1);

    pr_info("Driver unloaded cleanly.\n");
}

module_init(ring_module_init);
module_exit(ring_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Deepak Pandey");
MODULE_DESCRIPTION("Chapter 07: Safe Memory Transfer & Circular Ring Buffer");
MODULE_VERSION("1.0.0");
