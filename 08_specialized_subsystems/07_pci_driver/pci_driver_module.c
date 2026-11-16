// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file pci_driver_module.c
 * @brief Chapter 30: PCI / PCIe Subsystem & MSI-X Interrupts
 * @author Deepak Pandey
 *
 * This module demonstrates:
 *  - Architecture of the Linux PCI/PCIe Subsystem (struct pci_driver, struct pci_dev).
 *  - PCI device ID matching tables (struct pci_device_id, MODULE_DEVICE_TABLE).
 *  - Device lifecycle: pci_enable_device(), pci_set_master(), pci_request_regions().
 *  - Base Address Register (BAR) inspection and memory-mapped I/O (pci_iomap).
 *  - Allocating Message Signaled Interrupts (MSI / MSI-X) via pci_alloc_irq_vectors().
 *  - Character device node (/dev/pci_demo_dev) for user-space BAR register access.
 *  - Integrated loopback simulation mode for testing without physical PCI add-in cards.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/device.h>

#define DRIVER_NAME       "mock_pci_driver"
#define DEV_NAME          "pci_demo_dev"
#define SIMULATED_BAR_SZ  4096

/* Target device: QEMU EDU Educational PCI device or mock test ID */
#define PCI_VENDOR_ID_EDU 0x1234
#define PCI_DEVICE_ID_EDU 0x11e8

static const struct pci_device_id mock_pci_ids[] = {
    { PCI_DEVICE(PCI_VENDOR_ID_EDU, PCI_DEVICE_ID_EDU) },
    { } /* Null terminator */
};
MODULE_DEVICE_TABLE(pci, mock_pci_ids);

struct pci_demo_dev {
    struct pci_dev *pdev;
    void __iomem *bar0_mmio;
    resource_size_t bar0_start;
    resource_size_t bar0_len;

    /* Software Simulated BAR for systems without QEMU EDU device */
    u8 *sim_bar_memory;
    bool is_hardware_bound;

    /* Interrupt context */
    int irq_line;
    u64 interrupt_count;

    /* Character device */
    dev_t dev_num;
    struct cdev cdev;
    struct class *class;
    struct device *device;
    struct mutex lock;

    /* Telemetry statistics */
    u64 bar_reads;
    u64 bar_writes;
};

static struct pci_demo_dev *g_pci_dev;

static irqreturn_t pci_demo_isr(int irq, void *dev_id)
{
    struct pci_demo_dev *dev = dev_id;

    dev->interrupt_count++;
    pr_info("PCI Interrupt received on IRQ %d (total: %llu)\n", irq, dev->interrupt_count);
    return IRQ_HANDLED;
}

static int pci_demo_open(struct inode *inode, struct file *file)
{
    file->private_data = g_pci_dev;
    return 0;
}

static int pci_demo_release(struct inode *inode, struct file *file)
{
    return 0;
}

static ssize_t pci_demo_read(struct file *file, char __user *user_buf, size_t count, loff_t *ppos)
{
    struct pci_demo_dev *dev = file->private_data;
    size_t to_copy;

    if (!dev)
        return -ENODEV;

    if (*ppos >= SIMULATED_BAR_SZ)
        return 0;

    to_copy = min(count, (size_t)(SIMULATED_BAR_SZ - *ppos));

    if (mutex_lock_interruptible(&dev->lock))
        return -ERESTARTSYS;

    if (dev->is_hardware_bound && dev->bar0_mmio) {
        /* Read from real hardware MMIO */
        u8 temp[256];
        size_t chunk = min(to_copy, sizeof(temp));
        memcpy_fromio(temp, dev->bar0_mmio + *ppos, chunk);
        if (copy_to_user(user_buf, temp, chunk)) {
            mutex_unlock(&dev->lock);
            return -EFAULT;
        }
        to_copy = chunk;
    } else {
        /* Read from simulated BAR memory */
        if (copy_to_user(user_buf, dev->sim_bar_memory + *ppos, to_copy)) {
            mutex_unlock(&dev->lock);
            return -EFAULT;
        }
    }

    dev->bar_reads++;
    *ppos += to_copy;
    mutex_unlock(&dev->lock);

    return to_copy;
}

static ssize_t pci_demo_write(struct file *file, const char __user *user_buf, size_t count, loff_t *ppos)
{
    struct pci_demo_dev *dev = file->private_data;
    size_t to_copy;

    if (!dev)
        return -ENODEV;

    if (*ppos >= SIMULATED_BAR_SZ)
        return -ENOSPC;

    to_copy = min(count, (size_t)(SIMULATED_BAR_SZ - *ppos));

    if (mutex_lock_interruptible(&dev->lock))
        return -ERESTARTSYS;

    if (dev->is_hardware_bound && dev->bar0_mmio) {
        /* Write to real hardware MMIO */
        u8 temp[256];
        size_t chunk = min(to_copy, sizeof(temp));
        if (copy_from_user(temp, user_buf, chunk)) {
            mutex_unlock(&dev->lock);
            return -EFAULT;
        }
        memcpy_toio(dev->bar0_mmio + *ppos, temp, chunk);
        to_copy = chunk;
    } else {
        /* Write to simulated BAR memory */
        if (copy_from_user(dev->sim_bar_memory + *ppos, user_buf, to_copy)) {
            mutex_unlock(&dev->lock);
            return -EFAULT;
        }

        /* If writing offset 0x04, simulate software IRQ assertion */
        if (*ppos <= 0x04 && (*ppos + to_copy) >= 0x04) {
            dev->interrupt_count++;
            pr_info("Simulated PCI MSI interrupt generated! (Count: %llu)\n",
                    dev->interrupt_count);
        }
    }

    dev->bar_writes++;
    *ppos += to_copy;
    mutex_unlock(&dev->lock);

    return to_copy;
}

static const struct file_operations pci_demo_fops = {
    .owner   = THIS_MODULE,
    .open    = pci_demo_open,
    .release = pci_demo_release,
    .read    = pci_demo_read,
    .write   = pci_demo_write,
};

/* Sysfs Statistics Attribute */
static ssize_t pci_stats_show(struct device *device, struct device_attribute *attr, char *buf)
{
    struct pci_demo_dev *dev = g_pci_dev;

    if (!dev)
        return -ENODEV;

    return sysfs_emit(buf,
                      "Mode:               %s\n"
                      "BAR0 Physical Base: 0x%llx\n"
                      "BAR0 Length:        %llu bytes\n"
                      "IRQ Line:           %d\n"
                      "Interrupts Fired:   %llu\n"
                      "BAR Reads:          %llu\n"
                      "BAR Writes:         %llu\n",
                      dev->is_hardware_bound ? "Physical Hardware PCI" : "Simulated Virtual PCI",
                      (unsigned long long)dev->bar0_start,
                      (unsigned long long)dev->bar0_len,
                      dev->irq_line,
                      dev->interrupt_count,
                      dev->bar_reads,
                      dev->bar_writes);
}
static DEVICE_ATTR_RO(pci_stats);

/**
 * @brief PCI core probe callback
 */
static int mock_pci_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
    int ret;
    struct pci_demo_dev *dev = g_pci_dev;

    pr_info("PCI device detected! VID:0x%04x PID:0x%04x\n", pdev->vendor, pdev->device);

    if (!dev)
        return -ENODEV;

    mutex_lock(&dev->lock);

    /* 1. Enable PCI device */
    ret = pci_enable_device(pdev);
    if (ret) {
        pr_err("pci_enable_device failed (%d)\n", ret);
        mutex_unlock(&dev->lock);
        return ret;
    }

    /* 2. Enable bus mastering for DMA transactions */
    pci_set_master(pdev);

    /* 3. Reserve PCI I/O and memory BAR regions */
    ret = pci_request_regions(pdev, DRIVER_NAME);
    if (ret) {
        pr_err("pci_request_regions failed (%d)\n", ret);
        goto err_disable;
    }

    /* 4. Inspect and map BAR0 */
    dev->bar0_start = pci_resource_start(pdev, 0);
    dev->bar0_len   = pci_resource_len(pdev, 0);
    dev->bar0_mmio  = pci_iomap(pdev, 0, 0);
    if (!dev->bar0_mmio) {
        pr_err("pci_iomap failed on BAR0\n");
        ret = -EIO;
        goto err_release_regions;
    }

    /* 5. Allocate MSI Interrupts */
    ret = pci_alloc_irq_vectors(pdev, 1, 4, PCI_IRQ_ALL_TYPES);
    if (ret < 0) {
        pr_warn("pci_alloc_irq_vectors failed, falling back to legacy IRQ\n");
        dev->irq_line = pdev->irq;
    } else {
        dev->irq_line = pci_irq_vector(pdev, 0);
    }

    ret = request_irq(dev->irq_line, pci_demo_isr, IRQF_SHARED, DRIVER_NAME, dev);
    if (ret) {
        pr_err("request_irq failed on IRQ %d (%d)\n", dev->irq_line, ret);
        goto err_iounmap;
    }

    dev->pdev = pdev;
    dev->is_hardware_bound = true;
    pci_set_drvdata(pdev, dev);
    mutex_unlock(&dev->lock);

    pr_info("PCI device bound successfully! BAR0 at 0x%llx (size %llu)\n",
            (unsigned long long)dev->bar0_start, (unsigned long long)dev->bar0_len);
    return 0;

err_iounmap:
    pci_iounmap(pdev, dev->bar0_mmio);
err_release_regions:
    pci_release_regions(pdev);
err_disable:
    pci_clear_master(pdev);
    pci_disable_device(pdev);
    mutex_unlock(&dev->lock);
    return ret;
}

static void mock_pci_remove(struct pci_dev *pdev)
{
    struct pci_demo_dev *dev = pci_get_drvdata(pdev);

    pr_info("Removing PCI device binding\n");

    if (dev) {
        mutex_lock(&dev->lock);
        free_irq(dev->irq_line, dev);
        pci_free_irq_vectors(pdev);
        pci_iounmap(pdev, dev->bar0_mmio);
        pci_release_regions(pdev);
        pci_clear_master(pdev);
        pci_disable_device(pdev);
        dev->is_hardware_bound = false;
        dev->pdev = NULL;
        mutex_unlock(&dev->lock);
    }
}

static struct pci_driver mock_pci_driver_struct = {
    .name     = DRIVER_NAME,
    .id_table = mock_pci_ids,
    .probe    = mock_pci_probe,
    .remove   = mock_pci_remove,
};

static int __init mock_pci_init(void)
{
    int ret;
    struct pci_demo_dev *dev;

    pr_info("Initializing Chapter 30 PCI / PCIe Subsystem Module\n");

    dev = kzalloc(sizeof(*dev), GFP_KERNEL);
    if (!dev)
        return -ENOMEM;

    mutex_init(&dev->lock);
    dev->sim_bar_memory = kzalloc(SIMULATED_BAR_SZ, GFP_KERNEL);
    if (!dev->sim_bar_memory) {
        kfree(dev);
        return -ENOMEM;
    }

    /* Setup simulated BAR defaults: Device Identification Registers */
    dev->sim_bar_memory[0] = 0x34; /* Mock VID: 0x1234 */
    dev->sim_bar_memory[1] = 0x12;
    dev->sim_bar_memory[2] = 0xE8; /* Mock PID: 0x11E8 */
    dev->sim_bar_memory[3] = 0x11;
    dev->bar0_start = 0xFE800000;
    dev->bar0_len   = SIMULATED_BAR_SZ;
    dev->irq_line   = 42;

    /* Register Character Device */
    ret = alloc_chrdev_region(&dev->dev_num, 0, 1, DEV_NAME);
    if (ret < 0) {
        pr_err("alloc_chrdev_region failed (%d)\n", ret);
        goto err_free_sim;
    }

    cdev_init(&dev->cdev, &pci_demo_fops);
    dev->cdev.owner = THIS_MODULE;

    ret = cdev_add(&dev->cdev, dev->dev_num, 1);
    if (ret < 0) {
        pr_err("cdev_add failed (%d)\n", ret);
        goto err_chrdev;
    }

    dev->class = class_create("pci_demo_class");
    if (IS_ERR(dev->class)) {
        ret = PTR_ERR(dev->class);
        goto err_cdev;
    }

    dev->device = device_create(dev->class, NULL, dev->dev_num, NULL, DEV_NAME);
    if (IS_ERR(dev->device)) {
        ret = PTR_ERR(dev->device);
        goto err_class;
    }

    ret = device_create_file(dev->device, &dev_attr_pci_stats);
    if (ret)
        pr_warn("device_create_file failed (%d)\n", ret);

    g_pci_dev = dev;

    /* Register with PCI Core */
    ret = pci_register_driver(&mock_pci_driver_struct);
    if (ret) {
        pr_warn("pci_register_driver failed (%d), running in pure virtual mode\n", ret);
    }

    pr_info("PCI Driver initialized. Character Device: /dev/%s\n", DEV_NAME);
    return 0;

err_class:
    class_destroy(dev->class);
err_cdev:
    cdev_del(&dev->cdev);
err_chrdev:
    unregister_chrdev_region(dev->dev_num, 1);
err_free_sim:
    kfree(dev->sim_bar_memory);
    kfree(dev);
    return ret;
}

static void __exit mock_pci_exit(void)
{
    pr_info("Unloading Chapter 30 PCI / PCIe Subsystem Module\n");

    pci_unregister_driver(&mock_pci_driver_struct);

    if (g_pci_dev) {
        device_remove_file(g_pci_dev->device, &dev_attr_pci_stats);
        device_destroy(g_pci_dev->class, g_pci_dev->dev_num);
        class_destroy(g_pci_dev->class);
        cdev_del(&g_pci_dev->cdev);
        unregister_chrdev_region(g_pci_dev->dev_num, 1);

        kfree(g_pci_dev->sim_bar_memory);
        kfree(g_pci_dev);
        g_pci_dev = NULL;
    }

    pr_info("PCI module cleanup complete\n");
}

module_init(mock_pci_init);
module_exit(mock_pci_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Deepak Pandey");
MODULE_DESCRIPTION("Chapter 30: PCI / PCIe Subsystem & MSI-X Interrupts");
MODULE_VERSION("1.0");
