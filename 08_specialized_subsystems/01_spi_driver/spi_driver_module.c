// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file spi_driver_module.c
 * @brief Chapter 24: SPI Bus Driver Subsystem (spi_controller, spi_driver, spi_transfer, spi_sync)
 * @author Deepak Pandey
 *
 * This module demonstrates:
 *  - Architecture of the Linux SPI (Serial Peripheral Interface) Subsystem.
 *  - Implementing an SPI Host Controller (struct spi_controller) with transfer_one callback.
 *  - Registering and matching SPI Client Protocol Drivers (struct spi_driver).
 *  - Instantiating SPI devices using struct spi_board_info and spi_new_device().
 *  - Full-duplex synchronous SPI transactions using spi_message and spi_transfer (spi_sync).
 *  - Simulating an SPI sensor device (Register reading, Manufacturer ID, Temperature telemetry).
 *  - Exposing character device (/dev/spi_sensor) and sysfs telemetry attributes to user-space.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/spi/spi.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/device.h>

#define DRIVER_NAME       "mock_spi_driver"
#define DEV_NAME          "spi_sensor"
#define SPI_CLIENT_NAME   "mock_spi_sensor"
#define SPI_BUS_NUM       42
#define SPI_MAX_SPEED_HZ  1000000
#define BUFFER_SIZE       256

/* Simulated SPI Command Codes */
#define SPI_CMD_READ_ID     0x9F
#define SPI_CMD_READ_TEMP   0x02
#define SPI_CMD_READ_STATUS 0x03
#define SPI_CMD_LOOPBACK    0xAA

#define MOCK_MANUFACTURER_ID 0xE0
#define MOCK_DEVICE_ID       0x77

/**
 * @brief Private data structure for SPI client driver
 */
struct mock_spi_dev {
    struct spi_device *spi;
    dev_t dev_num;
    struct cdev cdev;
    struct class *class;
    struct device *device;
    struct mutex lock;

    /* Telemetry statistics */
    u64 total_transfers;
    u64 total_tx_bytes;
    u64 total_rx_bytes;
};

static struct mock_spi_dev *g_spi_client_dev;
static struct spi_controller *g_spi_ctlr;
static struct spi_device *g_spi_board_dev;

/**
 * @brief Host Controller transfer_one callback
 * Executes full-duplex transfer for a single spi_transfer descriptor.
 */
static int mock_spi_transfer_one(struct spi_controller *ctlr, struct spi_device *spi,
                                 struct spi_transfer *xfer)
{
    const u8 *tx = xfer->tx_buf;
    u8 *rx = xfer->rx_buf;
    size_t len = xfer->len;
    size_t i;

    for (i = 0; i < len; ++i) {
        u8 send_byte = tx ? tx[i] : 0x00;
        u8 recv_byte = 0x00;

        /* Emulate mock SPI slave register logic */
        switch (send_byte) {
        case SPI_CMD_READ_ID:
            recv_byte = (i % 2 == 0) ? MOCK_MANUFACTURER_ID : MOCK_DEVICE_ID;
            break;
        case SPI_CMD_READ_TEMP:
            recv_byte = 27; /* 27 degrees Celsius */
            break;
        case SPI_CMD_READ_STATUS:
            recv_byte = 0x01; /* Sensor status: READY / NORMAL */
            break;
        case SPI_CMD_LOOPBACK:
            recv_byte = ~send_byte; /* Bitwise complement echo */
            break;
        default:
            recv_byte = send_byte + 1; /* Transformed response */
            break;
        }

        if (rx)
            rx[i] = recv_byte;
    }

    return 0; /* Synchronously completed */
}

/**
 * @brief Character device file operations for user-space interaction
 */
static int mock_spi_open(struct inode *inode, struct file *file)
{
    file->private_data = g_spi_client_dev;
    return 0;
}

static int mock_spi_release(struct inode *inode, struct file *file)
{
    return 0;
}

static ssize_t mock_spi_read(struct file *file, char __user *user_buf, size_t count, loff_t *ppos)
{
    struct mock_spi_dev *dev = file->private_data;
    u8 tx_buf[4] = { SPI_CMD_READ_TEMP, 0x00, 0x00, 0x00 };
    u8 rx_buf[4] = { 0 };
    int ret;
    char out_str[64];
    size_t out_len;

    if (!dev || !dev->spi)
        return -ENODEV;

    if (mutex_lock_interruptible(&dev->lock))
        return -ERESTARTSYS;

    /* Execute full-duplex SPI synchronous transfer */
    ret = spi_write_then_read(dev->spi, tx_buf, sizeof(tx_buf), rx_buf, sizeof(rx_buf));
    if (ret < 0) {
        mutex_unlock(&dev->lock);
        return ret;
    }

    dev->total_transfers++;
    dev->total_tx_bytes += sizeof(tx_buf);
    dev->total_rx_bytes += sizeof(rx_buf);

    out_len = snprintf(out_str, sizeof(out_str), "SPI Sensor Temp: %u C (Raw: 0x%02x)\n",
                       rx_buf[0], rx_buf[0]);

    if (*ppos >= out_len) {
        mutex_unlock(&dev->lock);
        return 0;
    }

    if (count < out_len - *ppos)
        out_len = count + *ppos;

    if (copy_to_user(user_buf, out_str + *ppos, out_len - *ppos)) {
        mutex_unlock(&dev->lock);
        return -EFAULT;
    }

    out_len -= *ppos;
    *ppos += out_len;
    mutex_unlock(&dev->lock);
    return out_len;
}

static ssize_t mock_spi_write(struct file *file, const char __user *user_buf, size_t count, loff_t *ppos)
{
    struct mock_spi_dev *dev = file->private_data;
    u8 ktx[BUFFER_SIZE];
    u8 krx[BUFFER_SIZE];
    size_t to_copy;
    struct spi_transfer xfer = {
        .tx_buf = ktx,
        .rx_buf = krx,
    };
    struct spi_message msg;
    int ret;

    if (!dev || !dev->spi)
        return -ENODEV;

    to_copy = min(count, (size_t)BUFFER_SIZE);

    if (mutex_lock_interruptible(&dev->lock))
        return -ERESTARTSYS;

    if (copy_from_user(ktx, user_buf, to_copy)) {
        mutex_unlock(&dev->lock);
        return -EFAULT;
    }

    xfer.len = to_copy;
    spi_message_init(&msg);
    spi_message_add_tail(&xfer, &msg);

    ret = spi_sync(dev->spi, &msg);
    if (ret < 0) {
        mutex_unlock(&dev->lock);
        return ret;
    }

    dev->total_transfers++;
    dev->total_tx_bytes += to_copy;
    dev->total_rx_bytes += to_copy;

    pr_info("Full-duplex SPI transfer executed (%zu bytes). First byte TX:0x%02x -> RX:0x%02x\n",
            to_copy, ktx[0], krx[0]);

    mutex_unlock(&dev->lock);
    return to_copy;
}

static const struct file_operations mock_spi_fops = {
    .owner   = THIS_MODULE,
    .open    = mock_spi_open,
    .release = mock_spi_release,
    .read    = mock_spi_read,
    .write   = mock_spi_write,
};

/* Sysfs Statistics Attribute */
static ssize_t spi_stats_show(struct device *device, struct device_attribute *attr, char *buf)
{
    struct mock_spi_dev *dev = g_spi_client_dev;

    if (!dev)
        return -ENODEV;

    return sysfs_emit(buf,
                      "Bus Number:       %d\n"
                      "Max Speed:        %u Hz\n"
                      "Mode:             0x%x\n"
                      "Total Transfers:  %llu\n"
                      "Total TX Bytes:   %llu\n"
                      "Total RX Bytes:   %llu\n",
                      dev->spi->controller->bus_num,
                      dev->spi->max_speed_hz,
                      dev->spi->mode,
                      dev->total_transfers,
                      dev->total_tx_bytes,
                      dev->total_rx_bytes);
}
static DEVICE_ATTR_RO(spi_stats);

/**
 * @brief SPI Client Driver probe callback
 */
static int mock_spi_probe(struct spi_device *spi)
{
    int ret;
    struct mock_spi_dev *dev;

    pr_info("SPI client probe called for device '%s' on bus %d, cs %d\n",
            spi->modalias, spi->controller->bus_num, spi_get_chipselect(spi, 0));

    dev = kzalloc(sizeof(*dev), GFP_KERNEL);
    if (!dev)
        return -ENOMEM;

    dev->spi = spi;
    mutex_init(&dev->lock);

    /* Register Character Device */
    ret = alloc_chrdev_region(&dev->dev_num, 0, 1, DEV_NAME);
    if (ret < 0) {
        pr_err("alloc_chrdev_region failed (%d)\n", ret);
        goto err_kfree;
    }

    cdev_init(&dev->cdev, &mock_spi_fops);
    dev->cdev.owner = THIS_MODULE;

    ret = cdev_add(&dev->cdev, dev->dev_num, 1);
    if (ret < 0) {
        pr_err("cdev_add failed (%d)\n", ret);
        goto err_chrdev;
    }

    dev->class = class_create("mock_spi_class");
    if (IS_ERR(dev->class)) {
        ret = PTR_ERR(dev->class);
        goto err_cdev;
    }

    dev->device = device_create(dev->class, &spi->dev, dev->dev_num, NULL, DEV_NAME);
    if (IS_ERR(dev->device)) {
        ret = PTR_ERR(dev->device);
        goto err_class;
    }

    ret = device_create_file(dev->device, &dev_attr_spi_stats);
    if (ret)
        pr_warn("device_create_file failed (%d)\n", ret);

    spi_set_drvdata(spi, dev);
    g_spi_client_dev = dev;

    pr_info("SPI client driver initialized successfully. Node: /dev/%s\n", DEV_NAME);
    return 0;

err_class:
    class_destroy(dev->class);
err_cdev:
    cdev_del(&dev->cdev);
err_chrdev:
    unregister_chrdev_region(dev->dev_num, 1);
err_kfree:
    kfree(dev);
    return ret;
}

/**
 * @brief SPI Client Driver remove callback
 */
static void mock_spi_remove(struct spi_device *spi)
{
    struct mock_spi_dev *dev = spi_get_drvdata(spi);

    pr_info("SPI client driver remove called\n");

    if (dev) {
        device_remove_file(dev->device, &dev_attr_spi_stats);
        device_destroy(dev->class, dev->dev_num);
        class_destroy(dev->class);
        cdev_del(&dev->cdev);
        unregister_chrdev_region(dev->dev_num, 1);
        kfree(dev);
        g_spi_client_dev = NULL;
    }

    pr_info("SPI client driver cleaned up\n");
}

static const struct spi_device_id mock_spi_ids[] = {
    { SPI_CLIENT_NAME, 0 },
    { }
};
MODULE_DEVICE_TABLE(spi, mock_spi_ids);

static struct spi_driver mock_spi_protocol_driver = {
    .driver = {
        .name  = DRIVER_NAME,
        .owner = THIS_MODULE,
    },
    .probe    = mock_spi_probe,
    .remove   = mock_spi_remove,
    .id_table = mock_spi_ids,
};

static int __init mock_spi_module_init(void)
{
    int ret;
    struct spi_board_info board_info = {
        .modalias     = SPI_CLIENT_NAME,
        .max_speed_hz = SPI_MAX_SPEED_HZ,
        .bus_num      = SPI_BUS_NUM,
        .chip_select  = 0,
        .mode         = SPI_MODE_0,
    };

    pr_info("Initializing Chapter 24 SPI Bus Subsystem Module\n");

    /* 1. Register SPI Protocol Driver */
    ret = spi_register_driver(&mock_spi_protocol_driver);
    if (ret) {
        pr_err("spi_register_driver failed (%d)\n", ret);
        return ret;
    }

    /* 2. Allocate and Register Virtual SPI Host Controller */
    g_spi_ctlr = spi_alloc_host(NULL, 0);
    if (!g_spi_ctlr) {
        pr_err("spi_alloc_host failed\n");
        spi_unregister_driver(&mock_spi_protocol_driver);
        return -ENOMEM;
    }

    g_spi_ctlr->bus_num            = SPI_BUS_NUM;
    g_spi_ctlr->num_chipselect     = 1;
    g_spi_ctlr->mode_bits          = SPI_CPOL | SPI_CPHA | SPI_CS_HIGH;
    g_spi_ctlr->bits_per_word_mask = SPI_BPW_MASK(8);
    g_spi_ctlr->transfer_one       = mock_spi_transfer_one;

    ret = spi_register_controller(g_spi_ctlr);
    if (ret) {
        pr_err("spi_register_controller failed (%d)\n", ret);
        spi_controller_put(g_spi_ctlr);
        spi_unregister_driver(&mock_spi_protocol_driver);
        return ret;
    }

    /* 3. Instantiate SPI client device on the virtual controller */
    g_spi_board_dev = spi_new_device(g_spi_ctlr, &board_info);
    if (!g_spi_board_dev) {
        pr_err("spi_new_device failed\n");
        spi_unregister_controller(g_spi_ctlr);
        spi_unregister_driver(&mock_spi_protocol_driver);
        return -ENODEV;
    }

    pr_info("SPI Controller and Client Device registered successfully on Bus %d\n", SPI_BUS_NUM);
    return 0;
}

static void __exit mock_spi_module_exit(void)
{
    pr_info("Unloading Chapter 24 SPI Bus Subsystem Module\n");

    if (g_spi_board_dev) {
        spi_unregister_device(g_spi_board_dev);
        g_spi_board_dev = NULL;
    }

    if (g_spi_ctlr) {
        spi_unregister_controller(g_spi_ctlr);
        g_spi_ctlr = NULL;
    }

    spi_unregister_driver(&mock_spi_protocol_driver);
    pr_info("Cleanup complete\n");
}

module_init(mock_spi_module_init);
module_exit(mock_spi_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Deepak Pandey");
MODULE_DESCRIPTION("Chapter 24: SPI Bus Driver Subsystem (spi_controller, spi_driver, full-duplex)");
MODULE_VERSION("1.0");
