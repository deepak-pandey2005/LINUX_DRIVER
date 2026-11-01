// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file mmap_module.c
 * @brief Chapter 12: Kernel Memory Management & Zero-Copy mmap Architecture
 * @author Deepak Pandey
 *
 * This module demonstrates:
 *  - Allocating physical, page-aligned kernel memory with get_zeroed_page().
 *  - Marking pages reserved (SetPageReserved) for memory mapping.
 *  - Calculating Page Frame Numbers (PFN) using virt_to_page() and page_to_pfn().
 *  - Mapping physical memory directly into user-space Virtual Memory Areas (VMA)
 *    using remap_pfn_range() and modern vm_flags_set().
 *  - Demonstrating true Zero-Copy bidirectional data transfer without copy_to/from_user.
 *  - Clean memory unmapping and page release with ClearPageReserved() and free_page().
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>

#include "mmap_ioctl.h"

#define DEVICE_NAME "zero_copy_mmap"
#define CLASS_NAME  "mmap_class"

#define SHARED_BUFFER_SIZE PAGE_SIZE

/**
 * @brief Zero-copy mmap driver device state
 */
struct mmap_dev {
    dev_t dev_num;
    struct cdev cdev;
    struct class *dev_class;
    struct device *device;

    /* Shared physical page */
    char *kbuf;
    struct page *kpage;
    struct mutex lock;
};

static struct mmap_dev g_mmap_dev;

static int mmap_dev_open(struct inode *inode, struct file *file)
{
    pr_info("Device opened by process %s [PID: %d]\n", current->comm, current->pid);
    return 0;
}

static int mmap_dev_release(struct inode *inode, struct file *file)
{
    pr_info("Device closed\n");
    return 0;
}

/**
 * @brief VFS mmap callback: maps kernel buffer pages into process address space
 */
static int mmap_dev_mmap(struct file *file, struct vm_area_struct *vma)
{
    unsigned long size = vma->vm_end - vma->vm_start;
    unsigned long pfn;
    int ret;

    /* Verify mapping size does not exceed allocated buffer */
    if (size > SHARED_BUFFER_SIZE) {
        pr_warn("mmap requested size %lu exceeds buffer size %lu\n", size, SHARED_BUFFER_SIZE);
        return -EINVAL;
    }

    /* Convert virtual address to Page Frame Number (PFN) */
    pfn = page_to_pfn(g_mmap_dev.kpage);

    /*
     * Modern Linux 6.3+: vm_flags_set replaces direct vma->vm_flags mutation.
     * VM_DONTEXPAND: Prevents mremap() from expanding beyond initial mapping.
     * VM_DONTDUMP: Excludes this shared driver page from user core dumps.
     */
    vm_flags_set(vma, VM_DONTEXPAND | VM_DONTDUMP);

    /*
     * Map kernel physical PFN range to user-space virtual addresses (vma->vm_start).
     * vm_page_prot specifies standard caching and permission attributes.
     */
    ret = remap_pfn_range(vma, vma->vm_start, pfn, size, vma->vm_page_prot);
    if (ret) {
        pr_err("remap_pfn_range failed with error %d\n", ret);
        return -EAGAIN;
    }

    pr_info("mmap successful: mapped %lu bytes at PFN 0x%lx to user VMA [0x%lx - 0x%lx]\n",
            size, pfn, vma->vm_start, vma->vm_end);
    return 0;
}

/**
 * @brief IOCTL handler to facilitate verification and inspection
 */
static long mmap_dev_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    u32 buf_size = SHARED_BUFFER_SIZE;
    u8 fill_byte;
    char inspect_buf[64];

    switch (cmd) {
    case MMAP_IOCTL_GET_BUF_SIZE:
        if (copy_to_user((void __user *)arg, &buf_size, sizeof(buf_size)))
            return -EFAULT;
        return 0;

    case MMAP_IOCTL_KERNEL_FILL:
        if (copy_from_user(&fill_byte, (void __user *)arg, sizeof(fill_byte)))
            return -EFAULT;

        mutex_lock(&g_mmap_dev.lock);
        memset(g_mmap_dev.kbuf, fill_byte, SHARED_BUFFER_SIZE);
        mutex_unlock(&g_mmap_dev.lock);

        pr_info("Kernel filled entire shared page with 0x%02x\n", fill_byte);
        return 0;

    case MMAP_IOCTL_KERNEL_READ_STR:
        mutex_lock(&g_mmap_dev.lock);
        strscpy(inspect_buf, g_mmap_dev.kbuf, sizeof(inspect_buf));
        mutex_unlock(&g_mmap_dev.lock);

        if (copy_to_user((void __user *)arg, inspect_buf, sizeof(inspect_buf)))
            return -EFAULT;
        return 0;

    default:
        return -ENOTTY;
    }
}

static const struct file_operations mmap_fops = {
    .owner          = THIS_MODULE,
    .open           = mmap_dev_open,
    .release        = mmap_dev_release,
    .mmap           = mmap_dev_mmap,
    .unlocked_ioctl = mmap_dev_ioctl,
};

static int __init mmap_module_init(void)
{
    int ret;

    pr_info("Initializing Zero-Copy mmap Driver (Chapter 12)\n");

    mutex_init(&g_mmap_dev.lock);

    /* 1. Allocate page-aligned kernel memory */
    g_mmap_dev.kbuf = (char *)get_zeroed_page(GFP_KERNEL);
    if (!g_mmap_dev.kbuf) {
        pr_err("Failed to allocate zeroed page\n");
        return -ENOMEM;
    }

    g_mmap_dev.kpage = virt_to_page(g_mmap_dev.kbuf);

    /* Pin the page so it is marked reserved and protected from paging */
    SetPageReserved(g_mmap_dev.kpage);

    /* Initialize page with default banner */
    snprintf(g_mmap_dev.kbuf, SHARED_BUFFER_SIZE, "HELLO FROM KERNEL ZERO-COPY BUFFER!\n");

    /* 2. Allocate character device region */
    ret = alloc_chrdev_region(&g_mmap_dev.dev_num, 0, 1, DEVICE_NAME);
    if (ret < 0) {
        pr_err("alloc_chrdev_region failed (%d)\n", ret);
        goto free_page;
    }

    /* 3. Register cdev */
    cdev_init(&g_mmap_dev.cdev, &mmap_fops);
    g_mmap_dev.cdev.owner = THIS_MODULE;

    ret = cdev_add(&g_mmap_dev.cdev, g_mmap_dev.dev_num, 1);
    if (ret < 0) {
        pr_err("cdev_add failed (%d)\n", ret);
        goto unregister_chrdev;
    }

    /* 4. Create class */
    g_mmap_dev.dev_class = class_create(CLASS_NAME);
    if (IS_ERR(g_mmap_dev.dev_class)) {
        ret = PTR_ERR(g_mmap_dev.dev_class);
        pr_err("class_create failed (%d)\n", ret);
        goto delete_cdev;
    }

    /* 5. Create device node */
    g_mmap_dev.device = device_create(g_mmap_dev.dev_class, NULL, g_mmap_dev.dev_num,
                                      NULL, DEVICE_NAME);
    if (IS_ERR(g_mmap_dev.device)) {
        ret = PTR_ERR(g_mmap_dev.device);
        pr_err("device_create failed (%d)\n", ret);
        goto destroy_class;
    }

    pr_info("Zero-copy driver ready: /dev/%s registered (Buffer Size: %lu bytes)\n",
            DEVICE_NAME, SHARED_BUFFER_SIZE);
    return 0;

destroy_class:
    class_destroy(g_mmap_dev.dev_class);
delete_cdev:
    cdev_del(&g_mmap_dev.cdev);
unregister_chrdev:
    unregister_chrdev_region(g_mmap_dev.dev_num, 1);
free_page:
    ClearPageReserved(g_mmap_dev.kpage);
    free_page((unsigned long)g_mmap_dev.kbuf);
    return ret;
}

static void __exit mmap_module_exit(void)
{
    pr_info("Unloading Zero-Copy mmap Driver\n");

    device_destroy(g_mmap_dev.dev_class, g_mmap_dev.dev_num);
    class_destroy(g_mmap_dev.dev_class);
    cdev_del(&g_mmap_dev.cdev);
    unregister_chrdev_region(g_mmap_dev.dev_num, 1);

    ClearPageReserved(g_mmap_dev.kpage);
    free_page((unsigned long)g_mmap_dev.kbuf);
    mutex_destroy(&g_mmap_dev.lock);

    pr_info("Driver unloaded cleanly\n");
}

module_init(mmap_module_init);
module_exit(mmap_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Deepak Pandey");
MODULE_DESCRIPTION("Chapter 12: Kernel Memory Management & Zero-Copy mmap Interface");
MODULE_VERSION("1.0");
