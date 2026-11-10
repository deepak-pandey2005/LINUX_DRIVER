// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file dma_engine_module.c
 * @brief Chapter 20: DMA Engine & Zero-Copy Hardware Streaming (Coherent & Streaming DMA)
 * @author Deepak Pandey
 *
 * This module demonstrates:
 *  - Setting hardware DMA addressing masks with dma_set_mask_and_coherent().
 *  - Allocating and managing Coherent / Consistent DMA buffers (dma_alloc_coherent).
 *  - Establishing Streaming DMA mappings (dma_map_single, dma_unmap_single).
 *  - CPU cache coherency management with dma_sync_single_for_cpu() and dma_sync_single_for_device().
 *  - Translating Virtual Addresses to hardware Bus Addresses (dma_addr_t).
 *  - Emulating a high-speed hardware DMA scatter/streaming engine.
 *  - Exposing DMA telemetry to user-space via /dev/dma_engine_demo.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/ktime.h>

#include "dma_ioctl.h"

#define DRIVER_NAME        "dma_engine_drv"
#define DEVICE_NAME        "dma_engine_dev"
#define CDEV_NAME          "dma_engine_demo"
#define CLASS_NAME         "dma_demo_class"

#define COHERENT_BUF_SIZE  4096
#define STREAMING_BUF_SIZE 4096

/**
 * @brief DMA driver private device state
 */
struct dma_demo_priv {
    struct platform_device *pdev;
    dev_t dev_num;
    struct cdev cdev;
    struct class *dev_class;
    struct device *cdevice;
    struct mutex lock;

    /* 1. Coherent DMA Buffer */
    void *coherent_cpu_addr;
    dma_addr_t coherent_dma_handle;

    /* 2. Streaming DMA Buffer */
    void *streaming_cpu_addr;
    dma_addr_t streaming_dma_handle;

    /* Transfer telemetry */
    u64 transfer_count;
    u64 last_transfer_time_ns;
};

static struct dma_demo_priv *g_dma_priv;

static int dma_demo_open(struct inode *inode, struct file *file)
{
    file->private_data = g_dma_priv;
    return 0;
}

static ssize_t dma_demo_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
    struct dma_demo_priv *priv = file->private_data;
    char kbuf[384];
    int len;

    if (*ppos > 0)
        return 0;

    mutex_lock(&priv->lock);
    len = snprintf(kbuf, sizeof(kbuf),
                   "============================================\n"
                   " Linux Kernel DMA Engine Status Report\n"
                   "============================================\n"
                   "Coherent CPU VA    : %p\n"
                   "Coherent DMA Bus   : 0x%llx (Size: %d bytes)\n"
                   "Streaming CPU VA   : %p\n"
                   "Streaming DMA Bus  : 0x%llx (Size: %d bytes)\n"
                   "Total DMA Transfers: %llu\n"
                   "Last Transfer Time : %llu ns\n"
                   "============================================\n",
                   priv->coherent_cpu_addr,
                   (unsigned long long)priv->coherent_dma_handle,
                   COHERENT_BUF_SIZE,
                   priv->streaming_cpu_addr,
                   (unsigned long long)priv->streaming_dma_handle,
                   STREAMING_BUF_SIZE,
                   priv->transfer_count,
                   priv->last_transfer_time_ns);
    mutex_unlock(&priv->lock);

    if (copy_to_user(buf, kbuf, len))
        return -EFAULT;

    *ppos += len;
    return len;
}

static long dma_demo_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    struct dma_demo_priv *priv = file->private_data;
    struct dma_status_info info;

    switch (cmd) {
    case DMA_IOCTL_START_TRANSFER:
        mutex_lock(&priv->lock);

        /*
         * Emulate a high-speed DMA hardware burst from Coherent buffer to Streaming buffer:
         * 1. Prepare streaming buffer for device access (invalidate CPU caches)
         */
        dma_sync_single_for_device(&priv->pdev->dev, priv->streaming_dma_handle,
                                   STREAMING_BUF_SIZE, DMA_BIDIRECTIONAL);

        /* 2. Simulated Hardware Engine moves bytes directly between physical addresses */
        memcpy(priv->streaming_cpu_addr, priv->coherent_cpu_addr, COHERENT_BUF_SIZE);

        /* 3. Re-synchronize streaming buffer for CPU read (flush device updates to CPU cache) */
        dma_sync_single_for_cpu(&priv->pdev->dev, priv->streaming_dma_handle,
                                STREAMING_BUF_SIZE, DMA_BIDIRECTIONAL);

        priv->transfer_count++;
        priv->last_transfer_time_ns = ktime_get_ns();
        mutex_unlock(&priv->lock);

        pr_info("Simulated DMA hardware transfer #%llu completed successfully\n", priv->transfer_count);
        return 0;

    case DMA_IOCTL_GET_STATUS:
        mutex_lock(&priv->lock);
        info.coherent_bus_addr     = priv->coherent_dma_handle;
        info.streaming_bus_addr    = priv->streaming_dma_handle;
        info.coherent_size         = COHERENT_BUF_SIZE;
        info.streaming_size        = STREAMING_BUF_SIZE;
        info.total_dma_transfers   = priv->transfer_count;
        info.last_transfer_time_ns = priv->last_transfer_time_ns;
        mutex_unlock(&priv->lock);

        if (copy_to_user((void __user *)arg, &info, sizeof(info)))
            return -EFAULT;
        return 0;

    default:
        return -ENOTTY;
    }
}

static const struct file_operations dma_demo_fops = {
    .owner          = THIS_MODULE,
    .open           = dma_demo_open,
    .read           = dma_demo_read,
    .unlocked_ioctl = dma_demo_ioctl,
};

static int dma_demo_probe(struct platform_device *pdev)
{
    struct dma_demo_priv *priv;
    int ret;

    dev_info(&pdev->dev, "DMA probe() invoked\n");

    /* 1. Configure DMA bit addressing mask (64-bit capable) */
    ret = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(64));
    if (ret) {
        dev_warn(&pdev->dev, "64-bit DMA not supported, falling back to 32-bit\n");
        ret = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(32));
        if (ret) {
            dev_err(&pdev->dev, "No suitable DMA mask found\n");
            return ret;
        }
    }

    priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
    if (!priv)
        return -ENOMEM;

    priv->pdev = pdev;
    mutex_init(&priv->lock);
    platform_set_drvdata(pdev, priv);
    g_dma_priv = priv;

    /* 2. Allocate Coherent DMA Memory */
    priv->coherent_cpu_addr = dma_alloc_coherent(&pdev->dev, COHERENT_BUF_SIZE,
                                                 &priv->coherent_dma_handle, GFP_KERNEL);
    if (!priv->coherent_cpu_addr) {
        dev_err(&pdev->dev, "dma_alloc_coherent failed\n");
        return -ENOMEM;
    }
    memset(priv->coherent_cpu_addr, 0xA5, COHERENT_BUF_SIZE);
    dev_info(&pdev->dev, "Coherent DMA buffer: CPU=%p, Bus=0x%llx\n",
             priv->coherent_cpu_addr, (unsigned long long)priv->coherent_dma_handle);

    /* 3. Allocate and establish Streaming DMA Mapping */
    priv->streaming_cpu_addr = kmalloc(STREAMING_BUF_SIZE, GFP_KERNEL);
    if (!priv->streaming_cpu_addr) {
        ret = -ENOMEM;
        goto free_coherent;
    }
    memset(priv->streaming_cpu_addr, 0x00, STREAMING_BUF_SIZE);

    priv->streaming_dma_handle = dma_map_single(&pdev->dev, priv->streaming_cpu_addr,
                                                STREAMING_BUF_SIZE, DMA_BIDIRECTIONAL);
    if (dma_mapping_error(&pdev->dev, priv->streaming_dma_handle)) {
        dev_err(&pdev->dev, "dma_map_single failed\n");
        ret = -EIO;
        goto free_streaming_mem;
    }
    dev_info(&pdev->dev, "Streaming DMA buffer: CPU=%p, Bus=0x%llx\n",
             priv->streaming_cpu_addr, (unsigned long long)priv->streaming_dma_handle);

    /* 4. Register character device */
    ret = alloc_chrdev_region(&priv->dev_num, 0, 1, CDEV_NAME);
    if (ret < 0)
        goto unmap_streaming;

    cdev_init(&priv->cdev, &dma_demo_fops);
    priv->cdev.owner = THIS_MODULE;
    ret = cdev_add(&priv->cdev, priv->dev_num, 1);
    if (ret < 0)
        goto unregister_chrdev;

    priv->dev_class = class_create(CLASS_NAME);
    if (IS_ERR(priv->dev_class)) {
        ret = PTR_ERR(priv->dev_class);
        goto del_cdev;
    }

    priv->cdevice = device_create(priv->dev_class, &pdev->dev, priv->dev_num, NULL, CDEV_NAME);
    if (IS_ERR(priv->cdevice)) {
        ret = PTR_ERR(priv->cdevice);
        goto destroy_class;
    }

    dev_info(&pdev->dev, "/dev/%s ready for DMA engine verification\n", CDEV_NAME);
    return 0;

destroy_class:
    class_destroy(priv->dev_class);
del_cdev:
    cdev_del(&priv->cdev);
unregister_chrdev:
    unregister_chrdev_region(priv->dev_num, 1);
unmap_streaming:
    dma_unmap_single(&pdev->dev, priv->streaming_dma_handle, STREAMING_BUF_SIZE, DMA_BIDIRECTIONAL);
free_streaming_mem:
    kfree(priv->streaming_cpu_addr);
free_coherent:
    dma_free_coherent(&pdev->dev, COHERENT_BUF_SIZE, priv->coherent_cpu_addr, priv->coherent_dma_handle);
    return ret;
}

static void dma_demo_remove(struct platform_device *pdev)
{
    struct dma_demo_priv *priv = platform_get_drvdata(pdev);

    dev_info(&pdev->dev, "remove() invoked\n");

    if (priv) {
        device_destroy(priv->dev_class, priv->dev_num);
        class_destroy(priv->dev_class);
        cdev_del(&priv->cdev);
        unregister_chrdev_region(priv->dev_num, 1);

        dma_unmap_single(&pdev->dev, priv->streaming_dma_handle, STREAMING_BUF_SIZE, DMA_BIDIRECTIONAL);
        kfree(priv->streaming_cpu_addr);
        dma_free_coherent(&pdev->dev, COHERENT_BUF_SIZE, priv->coherent_cpu_addr, priv->coherent_dma_handle);
        g_dma_priv = NULL;
    }
}

static struct platform_driver dma_demo_driver = {
    .probe  = dma_demo_probe,
    .remove = dma_demo_remove,
    .driver = {
        .name  = DRIVER_NAME,
        .owner = THIS_MODULE,
    },
};

static struct platform_device *demo_pdev;

static int __init dma_engine_init(void)
{
    int ret;

    pr_info("Initializing Chapter 20 DMA Engine Demonstration Module\n");

    ret = platform_driver_register(&dma_demo_driver);
    if (ret)
        return ret;

    demo_pdev = platform_device_register_simple(DRIVER_NAME, 0, NULL, 0);
    if (IS_ERR(demo_pdev)) {
        ret = PTR_ERR(demo_pdev);
        platform_driver_unregister(&dma_demo_driver);
        return ret;
    }

    pr_info("DMA engine module loaded and bound successfully\n");
    return 0;
}

static void __exit dma_engine_exit(void)
{
    pr_info("Unloading Chapter 20 DMA Engine Demonstration Module\n");

    if (demo_pdev)
        platform_device_unregister(demo_pdev);

    platform_driver_unregister(&dma_demo_driver);
    pr_info("Clean teardown completed\n");
}

module_init(dma_engine_init);
module_exit(dma_engine_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Deepak Pandey");
MODULE_DESCRIPTION("Chapter 20: DMA Engine & Zero-Copy Hardware Streaming");
MODULE_VERSION("1.0");
