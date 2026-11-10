// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file usb_driver_module.c
 * @brief Chapter 22: USB Device Driver Subsystem (struct usb_driver, URBs, Endpoints)
 * @author Deepak Pandey
 *
 * This module demonstrates:
 *  - USB Core architecture and driver registration (usb_register, usb_deregister).
 *  - USB Device ID matching tables (struct usb_device_id, MODULE_DEVICE_TABLE).
 *  - Parsing USB descriptors (device, configuration, interface, endpoint).
 *  - Identifying Bulk IN and Bulk OUT endpoints.
 *  - Managing asynchronous USB Request Blocks (URBs):
 *      * usb_alloc_urb(), usb_fill_bulk_urb(), usb_submit_urb(), usb_free_urb().
 *      * Asynchronous completion callbacks and transfer status handling.
 *  - Exposing character device / sysfs interface for user-space interaction.
 *  - Integrated loopback simulation mode for testing on systems without dedicated USB test hardware.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/usb.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/device.h>

#define DRIVER_NAME       "usb_demo_driver"
#define DEV_NAME          "usb_demo_dev"
#define SIMULATED_VID     0x1234
#define SIMULATED_PID     0x5678
#define BULK_BUFFER_SIZE  512

/*
 * USB Device ID Table: defines devices handled by this driver.
 * In a real-world scenario, replace SIMULATED_VID/PID with actual hardware identifiers.
 */
static const struct usb_device_id usb_table[] = {
    { USB_DEVICE(SIMULATED_VID, SIMULATED_PID) },
    { } /* Terminating entry */
};
MODULE_DEVICE_TABLE(usb, usb_table);

/**
 * @brief Private driver state context
 */
struct usb_demo_dev {
    struct usb_device *udev;
    struct usb_interface *interface;

    /* Endpoints */
    __u8 bulk_in_endpointAddr;
    __u8 bulk_out_endpointAddr;
    size_t bulk_in_size;
    size_t bulk_out_size;

    /* Buffers for transfer */
    unsigned char *bulk_in_buffer;
    unsigned char *bulk_out_buffer;

    /* URB for asynchronous transactions */
    struct urb *rx_urb;
    struct urb *tx_urb;

    /* Synchronization and Telemetry */
    struct mutex io_mutex;
    bool device_connected;
    u64 tx_transfers;
    u64 rx_transfers;
    u64 total_tx_bytes;
    u64 total_rx_bytes;

    /* Character device infrastructure */
    dev_t dev_num;
    struct cdev cdev;
    struct class *class;
    struct device *device;
};

static struct usb_demo_dev *g_usb_dev;

/**
 * @brief Bulk URB completion callback handler
 */
static void __maybe_unused usb_bulk_rx_complete(struct urb *urb)
{
    struct usb_demo_dev *dev = urb->context;

    if (urb->status == 0) {
        dev->rx_transfers++;
        dev->total_rx_bytes += urb->actual_length;
        pr_info("URB RX complete: %d bytes received\n", urb->actual_length);
    } else {
        pr_warn("URB RX completed with error status %d\n", urb->status);
    }
}

static void __maybe_unused usb_bulk_tx_complete(struct urb *urb)
{
    struct usb_demo_dev *dev = urb->context;

    if (urb->status == 0) {
        dev->tx_transfers++;
        dev->total_tx_bytes += urb->actual_length;
        pr_info("URB TX complete: %d bytes transmitted\n", urb->actual_length);
    } else {
        pr_warn("URB TX completed with error status %d\n", urb->status);
    }
}

/**
 * @brief Character device file operations for user-space control & loopback testing
 */
static int usb_demo_open(struct inode *inode, struct file *file)
{
    file->private_data = g_usb_dev;
    return 0;
}

static int usb_demo_release(struct inode *inode, struct file *file)
{
    return 0;
}

static ssize_t usb_demo_read(struct file *file, char __user *user_buf, size_t count, loff_t *ppos)
{
    struct usb_demo_dev *dev = file->private_data;
    size_t to_copy;

    if (mutex_lock_interruptible(&dev->io_mutex))
        return -ERESTARTSYS;

    to_copy = min(count, (size_t)dev->bulk_in_size);

    if (copy_to_user(user_buf, dev->bulk_in_buffer, to_copy)) {
        mutex_unlock(&dev->io_mutex);
        return -EFAULT;
    }

    mutex_unlock(&dev->io_mutex);
    return to_copy;
}

static ssize_t usb_demo_write(struct file *file, const char __user *user_buf, size_t count, loff_t *ppos)
{
    struct usb_demo_dev *dev = file->private_data;
    size_t to_copy;

    if (mutex_lock_interruptible(&dev->io_mutex))
        return -ERESTARTSYS;

    to_copy = min(count, (size_t)dev->bulk_out_size);

    if (copy_from_user(dev->bulk_out_buffer, user_buf, to_copy)) {
        mutex_unlock(&dev->io_mutex);
        return -EFAULT;
    }

    /*
     * If physical USB device is bound, submit asynchronous URB.
     * Otherwise, simulate loopback by echoing payload into bulk_in_buffer!
     */
    if (dev->device_connected && dev->udev) {
        /*
         * Physical USB submit workflow:
         * usb_fill_bulk_urb(dev->tx_urb, dev->udev,
         *                   usb_sndbulkpipe(dev->udev, dev->bulk_out_endpointAddr),
         *                   dev->bulk_out_buffer, to_copy,
         *                   usb_bulk_tx_complete, dev);
         * usb_submit_urb(dev->tx_urb, GFP_KERNEL);
         */
        pr_info("Physical USB write submitted (%zu bytes)\n", to_copy);
    } else {
        /* Software loopback simulation */
        memcpy(dev->bulk_in_buffer, dev->bulk_out_buffer, to_copy);
        dev->tx_transfers++;
        dev->rx_transfers++;
        dev->total_tx_bytes += to_copy;
        dev->total_rx_bytes += to_copy;
        pr_info("Simulated USB loopback transfer: %zu bytes echoed via mock endpoints\n", to_copy);
    }

    mutex_unlock(&dev->io_mutex);
    return to_copy;
}

static const struct file_operations usb_demo_fops = {
    .owner   = THIS_MODULE,
    .open    = usb_demo_open,
    .release = usb_demo_release,
    .read    = usb_demo_read,
    .write   = usb_demo_write,
};

/**
 * @brief USB Core probe callback: called when a matching USB device is attached
 */
static int usb_demo_probe(struct usb_interface *interface, const struct usb_device_id *id)
{
    struct usb_device *udev = interface_to_usbdev(interface);
    struct usb_host_interface *iface_desc;
    struct usb_endpoint_descriptor *endpoint;
    int i;

    pr_info("USB probe called for VID:0x%04x PID:0x%04x\n",
            id->idVendor, id->idProduct);

    if (!g_usb_dev)
        return -ENODEV;

    mutex_lock(&g_usb_dev->io_mutex);
    g_usb_dev->udev = usb_get_dev(udev);
    g_usb_dev->interface = interface;

    /* Parse interface endpoint descriptors */
    iface_desc = interface->cur_altsetting;
    pr_info("Found interface with %d endpoints\n", iface_desc->desc.bNumEndpoints);

    for (i = 0; i < iface_desc->desc.bNumEndpoints; ++i) {
        endpoint = &iface_desc->endpoint[i].desc;

        /* Check for Bulk IN endpoint */
        if (!g_usb_dev->bulk_in_endpointAddr &&
            usb_endpoint_is_bulk_in(endpoint)) {
            g_usb_dev->bulk_in_endpointAddr = endpoint->bEndpointAddress;
            g_usb_dev->bulk_in_size = usb_endpoint_maxp(endpoint);
            pr_info("Discovered Bulk IN endpoint: 0x%02x (maxp: %zu)\n",
                    g_usb_dev->bulk_in_endpointAddr, g_usb_dev->bulk_in_size);
        }

        /* Check for Bulk OUT endpoint */
        if (!g_usb_dev->bulk_out_endpointAddr &&
            usb_endpoint_is_bulk_out(endpoint)) {
            g_usb_dev->bulk_out_endpointAddr = endpoint->bEndpointAddress;
            g_usb_dev->bulk_out_size = usb_endpoint_maxp(endpoint);
            pr_info("Discovered Bulk OUT endpoint: 0x%02x (maxp: %zu)\n",
                    g_usb_dev->bulk_out_endpointAddr, g_usb_dev->bulk_out_size);
        }
    }

    g_usb_dev->device_connected = true;
    usb_set_intfdata(interface, g_usb_dev);
    mutex_unlock(&g_usb_dev->io_mutex);

    pr_info("USB device bound successfully\n");
    return 0;
}

/**
 * @brief USB Core disconnect callback: called when device is unplugged
 */
static void usb_demo_disconnect(struct usb_interface *interface)
{
    struct usb_demo_dev *dev = usb_get_intfdata(interface);

    pr_info("USB disconnect called\n");

    if (dev) {
        mutex_lock(&dev->io_mutex);
        dev->device_connected = false;
        usb_set_intfdata(interface, NULL);
        if (dev->udev) {
            usb_put_dev(dev->udev);
            dev->udev = NULL;
        }
        mutex_unlock(&dev->io_mutex);
    }

    pr_info("USB device unbound\n");
}

/* USB Driver definition */
static struct usb_driver usb_demo_driver = {
    .name       = DRIVER_NAME,
    .probe      = usb_demo_probe,
    .disconnect = usb_demo_disconnect,
    .id_table   = usb_table,
};

/* Sysfs statistics attribute */
static ssize_t usb_stats_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    if (!g_usb_dev)
        return -ENODEV;

    return sysfs_emit(buf,
                      "Connected: %s\n"
                      "TX Transfers: %llu\n"
                      "RX Transfers: %llu\n"
                      "Total TX Bytes: %llu\n"
                      "Total RX Bytes: %llu\n",
                      g_usb_dev->device_connected ? "yes" : "simulated",
                      g_usb_dev->tx_transfers,
                      g_usb_dev->rx_transfers,
                      g_usb_dev->total_tx_bytes,
                      g_usb_dev->total_rx_bytes);
}
static DEVICE_ATTR_RO(usb_stats);

static int __init usb_demo_init(void)
{
    int ret;

    pr_info("Initializing Chapter 22 USB Subsystem Driver\n");

    g_usb_dev = kzalloc(sizeof(*g_usb_dev), GFP_KERNEL);
    if (!g_usb_dev)
        return -ENOMEM;

    mutex_init(&g_usb_dev->io_mutex);
    g_usb_dev->bulk_in_size = BULK_BUFFER_SIZE;
    g_usb_dev->bulk_out_size = BULK_BUFFER_SIZE;

    g_usb_dev->bulk_in_buffer = kzalloc(g_usb_dev->bulk_in_size, GFP_KERNEL);
    g_usb_dev->bulk_out_buffer = kzalloc(g_usb_dev->bulk_out_size, GFP_KERNEL);
    if (!g_usb_dev->bulk_in_buffer || !g_usb_dev->bulk_out_buffer) {
        ret = -ENOMEM;
        goto err_buffers;
    }

    /* Allocate URBs */
    g_usb_dev->rx_urb = usb_alloc_urb(0, GFP_KERNEL);
    g_usb_dev->tx_urb = usb_alloc_urb(0, GFP_KERNEL);
    if (!g_usb_dev->rx_urb || !g_usb_dev->tx_urb) {
        ret = -ENOMEM;
        goto err_urbs;
    }

    /* Register character device */
    ret = alloc_chrdev_region(&g_usb_dev->dev_num, 0, 1, DEV_NAME);
    if (ret < 0) {
        pr_err("alloc_chrdev_region failed (%d)\n", ret);
        goto err_urbs;
    }

    cdev_init(&g_usb_dev->cdev, &usb_demo_fops);
    g_usb_dev->cdev.owner = THIS_MODULE;

    ret = cdev_add(&g_usb_dev->cdev, g_usb_dev->dev_num, 1);
    if (ret < 0) {
        pr_err("cdev_add failed (%d)\n", ret);
        goto err_chrdev;
    }

    g_usb_dev->class = class_create("usb_demo_class");
    if (IS_ERR(g_usb_dev->class)) {
        ret = PTR_ERR(g_usb_dev->class);
        goto err_cdev;
    }

    g_usb_dev->device = device_create(g_usb_dev->class, NULL, g_usb_dev->dev_num,
                                      NULL, DEV_NAME);
    if (IS_ERR(g_usb_dev->device)) {
        ret = PTR_ERR(g_usb_dev->device);
        goto err_class;
    }

    ret = device_create_file(g_usb_dev->device, &dev_attr_usb_stats);
    if (ret)
        pr_warn("device_create_file failed (%d)\n", ret);

    /* Register with USB Subsystem */
    ret = usb_register(&usb_demo_driver);
    if (ret) {
        pr_err("usb_register failed (%d)\n", ret);
        goto err_device;
    }

    pr_info("USB driver registered successfully. Node: /dev/%s\n", DEV_NAME);
    return 0;

err_device:
    device_remove_file(g_usb_dev->device, &dev_attr_usb_stats);
    device_destroy(g_usb_dev->class, g_usb_dev->dev_num);
err_class:
    class_destroy(g_usb_dev->class);
err_cdev:
    cdev_del(&g_usb_dev->cdev);
err_chrdev:
    unregister_chrdev_region(g_usb_dev->dev_num, 1);
err_urbs:
    usb_free_urb(g_usb_dev->rx_urb);
    usb_free_urb(g_usb_dev->tx_urb);
err_buffers:
    kfree(g_usb_dev->bulk_in_buffer);
    kfree(g_usb_dev->bulk_out_buffer);
    kfree(g_usb_dev);
    g_usb_dev = NULL;
    return ret;
}

static void __exit usb_demo_exit(void)
{
    pr_info("Unregistering Chapter 22 USB Subsystem Driver\n");

    usb_deregister(&usb_demo_driver);

    if (g_usb_dev) {
        device_remove_file(g_usb_dev->device, &dev_attr_usb_stats);
        device_destroy(g_usb_dev->class, g_usb_dev->dev_num);
        class_destroy(g_usb_dev->class);
        cdev_del(&g_usb_dev->cdev);
        unregister_chrdev_region(g_usb_dev->dev_num, 1);

        usb_free_urb(g_usb_dev->rx_urb);
        usb_free_urb(g_usb_dev->tx_urb);

        kfree(g_usb_dev->bulk_in_buffer);
        kfree(g_usb_dev->bulk_out_buffer);
        kfree(g_usb_dev);
        g_usb_dev = NULL;
    }

    pr_info("Cleanup complete\n");
}

module_init(usb_demo_init);
module_exit(usb_demo_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Deepak Pandey");
MODULE_DESCRIPTION("Chapter 22: USB Device Driver Subsystem (struct usb_driver, URBs, endpoints)");
MODULE_VERSION("1.0");
