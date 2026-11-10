// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file i2c_sensor_module.c
 * @brief Chapter 19: I2C Client Drivers & Simulated Sensor Subsystem
 * @author Deepak Pandey
 *
 * This module demonstrates:
 *  - Implementing an I2C client driver with struct i2c_driver, probe(), and modern void-returning remove().
 *  - Performing SMBus / I2C transactions (i2c_smbus_read_byte_data, i2c_smbus_read_word_data, i2c_smbus_write_byte_data).
 *  - Providing an in-kernel virtual I2C bus adapter (struct i2c_adapter, struct i2c_algorithm) to allow testing on any hardware.
 *  - Instantiating I2C client devices dynamically with i2c_new_client_device().
 *  - Translating raw 11-bit twos-complement sensor registers into real milli-Celsius temperatures.
 *  - Exposing sensor telemetry to user-space via /dev/i2c_temp_sensor and IOCTL.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>

#include "i2c_sensor_ioctl.h"

#define DRIVER_NAME  "lm75_temp_sensor"
#define DEVICE_NAME  "i2c_temp_sensor"
#define CLASS_NAME   "i2c_sensor_class"
#define I2C_ADDR_7B  0x48

/* LM75 Register Definitions */
#define LM75_REG_TEMP   0x00
#define LM75_REG_CONF   0x01
#define LM75_REG_THYST  0x02
#define LM75_REG_TOS    0x03
#define LM75_REG_ID     0x07
#define LM75_CHIP_ID    0x75

/**
 * @brief Simulated hardware sensor register bank
 */
struct simulated_hw_sensor {
    struct mutex lock;
    u16 raw_temp;    /* 9-bit/11-bit fixed point */
    u8 config;
    u8 id;
};

static struct simulated_hw_sensor g_hw_sensor = {
    .raw_temp = 0x1980, /* 25.5 C (0x19 = 25, 0x80 = 0.5) */
    .config   = 0x00,
    .id       = LM75_CHIP_ID,
};

/* =========================================================================
 * 1. Virtual I2C Bus Adapter (Enables testing on systems without physical I2C)
 * ========================================================================= */

static int virtual_i2c_xfer(struct i2c_adapter *adap, struct i2c_msg *msgs, int num)
{
    int i;
    u8 reg_addr = 0;

    for (i = 0; i < num; i++) {
        struct i2c_msg *msg = &msgs[i];

        if (msg->addr != I2C_ADDR_7B)
            return -ENODEV;

        if (msg->flags & I2C_M_RD) {
            /* Master Read from Slave */
            if (reg_addr == LM75_REG_TEMP && msg->len >= 2) {
                mutex_lock(&g_hw_sensor.lock);
                /* Increment slightly on each read to simulate dynamic temperature */
                g_hw_sensor.raw_temp += 0x0010;
                msg->buf[0] = (g_hw_sensor.raw_temp >> 8) & 0xFF;
                msg->buf[1] = g_hw_sensor.raw_temp & 0xFF;
                mutex_unlock(&g_hw_sensor.lock);
            } else if (reg_addr == LM75_REG_CONF && msg->len >= 1) {
                msg->buf[0] = g_hw_sensor.config;
            } else if (reg_addr == LM75_REG_ID && msg->len >= 1) {
                msg->buf[0] = g_hw_sensor.id;
            } else if (msg->len >= 1) {
                msg->buf[0] = 0x00;
            }
        } else {
            /* Master Write to Slave */
            if (msg->len > 0) {
                reg_addr = msg->buf[0];
                if (msg->len > 1 && reg_addr == LM75_REG_CONF) {
                    mutex_lock(&g_hw_sensor.lock);
                    g_hw_sensor.config = msg->buf[1];
                    mutex_unlock(&g_hw_sensor.lock);
                }
            }
        }
    }

    return num;
}

static u32 virtual_i2c_func(struct i2c_adapter *adap)
{
    return I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL;
}

static const struct i2c_algorithm virtual_i2c_algo = {
    .master_xfer   = virtual_i2c_xfer,
    .functionality = virtual_i2c_func,
};

static struct i2c_adapter virtual_adapter = {
    .owner = THIS_MODULE,
    .name  = "Virtual I2C Adapter (Chapter 19)",
    .algo  = &virtual_i2c_algo,
};

/* =========================================================================
 * 2. I2C Client Driver Implementation
 * ========================================================================= */

struct i2c_sensor_priv {
    struct i2c_client *client;
    dev_t dev_num;
    struct cdev cdev;
    struct class *dev_class;
    struct device *cdevice;
    struct mutex lock;
};

static struct i2c_sensor_priv *g_sensor_priv;

static s32 lm75_read_temperature(struct i2c_client *client)
{
    int raw = i2c_smbus_read_word_swapped(client, LM75_REG_TEMP);
    if (raw < 0)
        return raw;

    /* LM75 raw format: MSB is integer Celsius, upper bit of LSB is 0.5 C */
    /* 9-bit resolution: (raw >> 7) * 500 milli-Celsius */
    return (raw >> 7) * 500;
}

static int i2c_sensor_open(struct inode *inode, struct file *file)
{
    file->private_data = g_sensor_priv;
    return 0;
}

static ssize_t i2c_sensor_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
    struct i2c_sensor_priv *priv = file->private_data;
    char kbuf[256];
    s32 temp_mC;
    int len, chip_id, conf;

    if (*ppos > 0)
        return 0;

    mutex_lock(&priv->lock);
    temp_mC = lm75_read_temperature(priv->client);
    chip_id = i2c_smbus_read_byte_data(priv->client, LM75_REG_ID);
    conf    = i2c_smbus_read_byte_data(priv->client, LM75_REG_CONF);
    mutex_unlock(&priv->lock);

    len = snprintf(kbuf, sizeof(kbuf),
                   "============================================\n"
                   " LM75 I2C Temperature Sensor Telemetry\n"
                   "============================================\n"
                   "I2C Bus Address    : 0x%02X\n"
                   "Chip ID            : 0x%02X\n"
                   "Configuration Reg  : 0x%02X\n"
                   "Temperature        : %d.%02d C (%d mC)\n"
                   "============================================\n",
                   priv->client->addr,
                   chip_id,
                   conf,
                   temp_mC / 1000, abs(temp_mC % 1000) / 10,
                   temp_mC);

    if (copy_to_user(buf, kbuf, len))
        return -EFAULT;

    *ppos += len;
    return len;
}

static long i2c_sensor_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    struct i2c_sensor_priv *priv = file->private_data;
    struct i2c_temp_data data;
    u8 conf_val;
    int ret;

    switch (cmd) {
    case I2C_IOCTL_GET_TEMP:
        mutex_lock(&priv->lock);
        data.temp_milli_celsius = lm75_read_temperature(priv->client);
        data.chip_id = i2c_smbus_read_byte_data(priv->client, LM75_REG_ID);
        data.config_reg = i2c_smbus_read_byte_data(priv->client, LM75_REG_CONF);
        mutex_unlock(&priv->lock);

        if (copy_to_user((void __user *)arg, &data, sizeof(data)))
            return -EFAULT;
        return 0;

    case I2C_IOCTL_SET_CONFIG:
        if (copy_from_user(&conf_val, (void __user *)arg, sizeof(conf_val)))
            return -EFAULT;

        mutex_lock(&priv->lock);
        ret = i2c_smbus_write_byte_data(priv->client, LM75_REG_CONF, conf_val);
        mutex_unlock(&priv->lock);
        return ret;

    default:
        return -ENOTTY;
    }
}

static const struct file_operations i2c_sensor_fops = {
    .owner          = THIS_MODULE,
    .open           = i2c_sensor_open,
    .read           = i2c_sensor_read,
    .unlocked_ioctl = i2c_sensor_ioctl,
};

static int lm75_probe(struct i2c_client *client)
{
    struct i2c_sensor_priv *priv;
    int ret, id;

    dev_info(&client->dev, "I2C probe() invoked for device at address 0x%02X\n", client->addr);

    /* Verify device presence by querying Chip ID */
    id = i2c_smbus_read_byte_data(client, LM75_REG_ID);
    if (id != LM75_CHIP_ID) {
        dev_err(&client->dev, "Unexpected Chip ID 0x%02X (expected 0x%02X)\n", id, LM75_CHIP_ID);
        return -ENODEV;
    }
    dev_info(&client->dev, "LM75 Sensor identified (Chip ID: 0x%02X)\n", id);

    priv = devm_kzalloc(&client->dev, sizeof(*priv), GFP_KERNEL);
    if (!priv)
        return -ENOMEM;

    priv->client = client;
    mutex_init(&priv->lock);
    i2c_set_clientdata(client, priv);
    g_sensor_priv = priv;

    /* Register character device */
    ret = alloc_chrdev_region(&priv->dev_num, 0, 1, DEVICE_NAME);
    if (ret < 0)
        return ret;

    cdev_init(&priv->cdev, &i2c_sensor_fops);
    priv->cdev.owner = THIS_MODULE;
    ret = cdev_add(&priv->cdev, priv->dev_num, 1);
    if (ret < 0)
        goto unregister_chrdev;

    priv->dev_class = class_create(CLASS_NAME);
    if (IS_ERR(priv->dev_class)) {
        ret = PTR_ERR(priv->dev_class);
        goto del_cdev;
    }

    priv->cdevice = device_create(priv->dev_class, &client->dev, priv->dev_num, NULL, DEVICE_NAME);
    if (IS_ERR(priv->cdevice)) {
        ret = PTR_ERR(priv->cdevice);
        goto destroy_class;
    }

    dev_info(&client->dev, "/dev/%s ready for sensor telemetry\n", DEVICE_NAME);
    return 0;

destroy_class:
    class_destroy(priv->dev_class);
del_cdev:
    cdev_del(&priv->cdev);
unregister_chrdev:
    unregister_chrdev_region(priv->dev_num, 1);
    return ret;
}

static void lm75_remove(struct i2c_client *client)
{
    struct i2c_sensor_priv *priv = i2c_get_clientdata(client);

    dev_info(&client->dev, "remove() invoked\n");

    if (priv) {
        device_destroy(priv->dev_class, priv->dev_num);
        class_destroy(priv->dev_class);
        cdev_del(&priv->cdev);
        unregister_chrdev_region(priv->dev_num, 1);
        g_sensor_priv = NULL;
    }
}

static const struct i2c_device_id lm75_id_table[] = {
    { DRIVER_NAME, 0 },
    { /* Sentinel */ }
};
MODULE_DEVICE_TABLE(i2c, lm75_id_table);

static struct i2c_driver lm75_driver = {
    .driver = {
        .name  = DRIVER_NAME,
        .owner = THIS_MODULE,
    },
    .probe    = lm75_probe,
    .remove   = lm75_remove,
    .id_table = lm75_id_table,
};

static struct i2c_client *demo_client;

static int __init i2c_sensor_init(void)
{
    int ret;
    struct i2c_board_info board_info = {
        I2C_BOARD_INFO(DRIVER_NAME, I2C_ADDR_7B),
    };

    pr_info("Initializing Chapter 19 I2C Sensor Driver & Virtual Adapter\n");
    mutex_init(&g_hw_sensor.lock);

    /* 1. Register virtual adapter */
    ret = i2c_add_adapter(&virtual_adapter);
    if (ret) {
        pr_err("i2c_add_adapter failed (%d)\n", ret);
        return ret;
    }

    /* 2. Register i2c driver */
    ret = i2c_add_driver(&lm75_driver);
    if (ret) {
        pr_err("i2c_add_driver failed (%d)\n", ret);
        i2c_del_adapter(&virtual_adapter);
        return ret;
    }

    /* 3. Instantiate client device on our virtual adapter to trigger probe() */
    demo_client = i2c_new_client_device(&virtual_adapter, &board_info);
    if (IS_ERR(demo_client)) {
        ret = PTR_ERR(demo_client);
        pr_err("i2c_new_client_device failed (%d)\n", ret);
        i2c_del_driver(&lm75_driver);
        i2c_del_adapter(&virtual_adapter);
        return ret;
    }

    pr_info("I2C sensor module loaded successfully\n");
    return 0;
}

static void __exit i2c_sensor_exit(void)
{
    pr_info("Unloading Chapter 19 I2C Sensor Driver\n");

    if (demo_client)
        i2c_unregister_device(demo_client);

    i2c_del_driver(&lm75_driver);
    i2c_del_adapter(&virtual_adapter);

    pr_info("Driver unloaded cleanly\n");
}

module_init(i2c_sensor_init);
module_exit(i2c_sensor_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Deepak Pandey");
MODULE_DESCRIPTION("Chapter 19: I2C Client Drivers & Simulated Sensor Subsystem");
MODULE_VERSION("1.0");

