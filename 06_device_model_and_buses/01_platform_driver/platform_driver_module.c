// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file platform_driver_module.c
 * @brief Chapter 16: Linux Device Model & Platform Drivers (probe, remove, resources)
 * @author Deepak Pandey
 *
 * This module demonstrates:
 *  - The Linux Device Model architecture: Busses, Devices, and Drivers.
 *  - Platform bus (pseudo-bus for non-discoverable on-SoC peripherals).
 *  - Creating and binding platform devices and platform drivers.
 *  - Resource acquisition: platform_get_resource() and platform_get_irq().
 *  - Managed resource allocation using devm_kzalloc().
 *  - Driver private state binding via platform_set_drvdata() and platform_get_drvdata().
 *  - Clean driver unbinding via modern void-returning remove() callback.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/ioport.h>

#define DRIVER_NAME "demo_platform_drv"
#define DEVICE_NAME "demo_platform_dev"
#define CDEV_NAME   "platform_demo"
#define CLASS_NAME  "platform_demo_class"

/* Simulated hardware memory-mapped IO window */
#define FAKE_REG_BASE 0x44E00000
#define FAKE_REG_SIZE 0x1000
#define FAKE_IRQ_NUM  42

/**
 * @brief Driver private device data structure
 */
struct platform_demo_priv {
    struct platform_device *pdev;
    struct resource *mem_res;
    int irq;
    dev_t dev_num;
    struct cdev cdev;
    struct class *dev_class;
    struct device *cdevice;
    u32 access_count;
};

/* File operations for user-space verification */
static int platform_demo_open(struct inode *inode, struct file *file)
{
    struct platform_demo_priv *priv = container_of(inode->i_cdev, struct platform_demo_priv, cdev);
    file->private_data = priv;
    priv->access_count++;
    return 0;
}

static ssize_t platform_demo_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
    struct platform_demo_priv *priv = file->private_data;
    char kbuf[256];
    int len;

    if (*ppos > 0)
        return 0;

    len = snprintf(kbuf, sizeof(kbuf),
                   "Platform Device: %s\n"
                   "Bound Driver   : %s\n"
                   "Base Memory    : 0x%llx - 0x%llx (Size: %llu bytes)\n"
                   "Assigned IRQ   : %d\n"
                   "Access Count   : %u\n",
                   priv->pdev->name,
                   DRIVER_NAME,
                   (unsigned long long)priv->mem_res->start,
                   (unsigned long long)priv->mem_res->end,
                   (unsigned long long)resource_size(priv->mem_res),
                   priv->irq,
                   priv->access_count);

    if (copy_to_user(buf, kbuf, len))
        return -EFAULT;

    *ppos += len;
    return len;
}

static const struct file_operations platform_demo_fops = {
    .owner   = THIS_MODULE,
    .open    = platform_demo_open,
    .read    = platform_demo_read,
};

/**
 * @brief Platform driver probe callback
 * Invoked by bus core when device name matches driver name!
 */
static int demo_platform_probe(struct platform_device *pdev)
{
    struct platform_demo_priv *priv;
    int ret;

    pr_info("probe() invoked for platform device '%s' (id=%d)\n", pdev->name, pdev->id);

    /* 1. Allocate device-managed private state */
    priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
    if (!priv)
        return -ENOMEM;

    priv->pdev = pdev;
    platform_set_drvdata(pdev, priv);

    /* 2. Retrieve hardware memory resource */
    priv->mem_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    if (!priv->mem_res) {
        dev_err(&pdev->dev, "Failed to get IORESOURCE_MEM\n");
        return -ENODEV;
    }
    dev_info(&pdev->dev, "Found MEM resource: 0x%llx - 0x%llx\n",
             (unsigned long long)priv->mem_res->start,
             (unsigned long long)priv->mem_res->end);

    /* 3. Retrieve hardware IRQ resource */
    priv->irq = platform_get_irq_optional(pdev, 0);
    if (priv->irq < 0) {
        dev_warn(&pdev->dev, "No IRQ assigned or platform_get_irq failed (%d)\n", priv->irq);
        priv->irq = FAKE_IRQ_NUM;
    } else {
        dev_info(&pdev->dev, "Found IRQ resource: %d\n", priv->irq);
    }

    /* 4. Register character device interface */
    ret = alloc_chrdev_region(&priv->dev_num, 0, 1, CDEV_NAME);
    if (ret < 0) {
        dev_err(&pdev->dev, "alloc_chrdev_region failed (%d)\n", ret);
        return ret;
    }

    cdev_init(&priv->cdev, &platform_demo_fops);
    priv->cdev.owner = THIS_MODULE;
    ret = cdev_add(&priv->cdev, priv->dev_num, 1);
    if (ret < 0) {
        dev_err(&pdev->dev, "cdev_add failed (%d)\n", ret);
        goto unregister_chrdev;
    }

    priv->dev_class = class_create(CLASS_NAME);
    if (IS_ERR(priv->dev_class)) {
        ret = PTR_ERR(priv->dev_class);
        dev_err(&pdev->dev, "class_create failed (%d)\n", ret);
        goto del_cdev;
    }

    priv->cdevice = device_create(priv->dev_class, &pdev->dev, priv->dev_num, NULL, CDEV_NAME);
    if (IS_ERR(priv->cdevice)) {
        ret = PTR_ERR(priv->cdevice);
        dev_err(&pdev->dev, "device_create failed (%d)\n", ret);
        goto destroy_class;
    }

    dev_info(&pdev->dev, "Device bound and /dev/%s node created successfully\n", CDEV_NAME);
    return 0;

destroy_class:
    class_destroy(priv->dev_class);
del_cdev:
    cdev_del(&priv->cdev);
unregister_chrdev:
    unregister_chrdev_region(priv->dev_num, 1);
    return ret;
}

/**
 * @brief Modern Linux 6.11+ remove callback returning void
 */
static void demo_platform_remove(struct platform_device *pdev)
{
    struct platform_demo_priv *priv = platform_get_drvdata(pdev);

    pr_info("remove() invoked for platform device '%s'\n", pdev->name);

    if (priv) {
        device_destroy(priv->dev_class, priv->dev_num);
        class_destroy(priv->dev_class);
        cdev_del(&priv->cdev);
        unregister_chrdev_region(priv->dev_num, 1);
    }

    pr_info("Device unbound cleanly\n");
}

static const struct platform_device_id demo_platform_ids[] = {
    { .name = DEVICE_NAME, },
    { /* Sentinel */ }
};
MODULE_DEVICE_TABLE(platform, demo_platform_ids);

static struct platform_driver demo_platform_driver = {
    .probe    = demo_platform_probe,
    .remove   = demo_platform_remove,
    .id_table = demo_platform_ids,
    .driver   = {
        .name = DRIVER_NAME,
        .owner = THIS_MODULE,
    },
};

/* Synthetic platform device instance for testing */
static struct resource demo_resources[] = {
    [0] = {
        .start = FAKE_REG_BASE,
        .end   = FAKE_REG_BASE + FAKE_REG_SIZE - 1,
        .flags = IORESOURCE_MEM,
    },
    [1] = {
        .start = FAKE_IRQ_NUM,
        .end   = FAKE_IRQ_NUM,
        .flags = IORESOURCE_IRQ,
    },
};

static struct platform_device *demo_pdev;

static int __init platform_demo_init(void)
{
    int ret;

    pr_info("Initializing Chapter 16 Platform Driver & Device\n");

    /* 1. Register the platform driver */
    ret = platform_driver_register(&demo_platform_driver);
    if (ret) {
        pr_err("platform_driver_register failed (%d)\n", ret);
        return ret;
    }

    /* 2. Register synthetic platform device to trigger bus matching & probe */
    demo_pdev = platform_device_register_resndata(NULL, DEVICE_NAME, 0,
                                                  demo_resources, ARRAY_SIZE(demo_resources),
                                                  NULL, 0);
    if (IS_ERR(demo_pdev)) {
        ret = PTR_ERR(demo_pdev);
        pr_err("platform_device_register_resndata failed (%d)\n", ret);
        platform_driver_unregister(&demo_platform_driver);
        return ret;
    }

    pr_info("Platform device and driver registered successfully\n");
    return 0;
}

static void __exit platform_demo_exit(void)
{
    pr_info("Unregistering Chapter 16 Platform Driver & Device\n");

    if (demo_pdev)
        platform_device_unregister(demo_pdev);

    platform_driver_unregister(&demo_platform_driver);
    pr_info("Clean teardown completed\n");
}

module_init(platform_demo_init);
module_exit(platform_demo_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Deepak Pandey");
MODULE_DESCRIPTION("Chapter 16: Linux Device Model & Platform Driver (probe, remove, resources)");
MODULE_VERSION("1.0");

