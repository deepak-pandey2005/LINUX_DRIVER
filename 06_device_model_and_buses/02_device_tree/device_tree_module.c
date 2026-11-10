// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file device_tree_module.c
 * @brief Chapter 17: Device Tree Simulation & OF Property Parsing (of_device_id, properties)
 * @author Deepak Pandey
 *
 * This module demonstrates:
 *  - Device Tree / Open Firmware matching table using struct of_device_id and MODULE_DEVICE_TABLE(of, ...).
 *  - Modern cross-platform unified device property API (<linux/property.h>).
 *  - Reading numeric properties (device_property_read_u32).
 *  - Reading string properties (device_property_read_string).
 *  - Reading boolean flag properties (device_property_read_bool).
 *  - Simulating Device Tree nodes on x86/non-OF kernels using software nodes.
 *  - Exposing parsed DT properties to user-space via a character device.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/property.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>

#define DRIVER_NAME "dt_sensor_drv"
#define DEVICE_NAME "packt_custom_sensor"
#define CDEV_NAME   "dt_sensor"
#define CLASS_NAME  "dt_sensor_class"

/**
 * @brief Driver private state containing parsed DT properties
 */
struct dt_sensor_priv {
    struct platform_device *pdev;
    dev_t dev_num;
    struct cdev cdev;
    struct class *dev_class;
    struct device *cdevice;

    /* Parsed Device Tree properties */
    u32 clock_freq_hz;
    u32 sampling_rate_hz;
    const char *sensor_label;
    bool power_saving_enabled;
    const char *matched_compatible;
};

static int dt_sensor_open(struct inode *inode, struct file *file)
{
    struct dt_sensor_priv *priv = container_of(inode->i_cdev, struct dt_sensor_priv, cdev);
    file->private_data = priv;
    return 0;
}

static ssize_t dt_sensor_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
    struct dt_sensor_priv *priv = file->private_data;
    char kbuf[384];
    int len;

    if (*ppos > 0)
        return 0;

    len = snprintf(kbuf, sizeof(kbuf),
                   "============================================\n"
                   " Device Tree / OF Property Inspection Demo\n"
                   "============================================\n"
                   "Compatible Match   : %s\n"
                   "Sensor Label       : %s\n"
                   "Bus Clock Frequency: %u Hz (%u kHz)\n"
                   "Sampling Rate      : %u Hz\n"
                   "Power Saving Mode  : %s\n"
                   "Device Node Type   : %s\n"
                   "============================================\n",
                   priv->matched_compatible ? priv->matched_compatible : "N/A",
                   priv->sensor_label ? priv->sensor_label : "unknown",
                   priv->clock_freq_hz, priv->clock_freq_hz / 1000,
                   priv->sampling_rate_hz,
                   priv->power_saving_enabled ? "ENABLED" : "DISABLED",
                   dev_of_node(&priv->pdev->dev) ? "Real OpenFirmware DT Node" : "Software Node Fwnode (x86/ACPI)");

    if (copy_to_user(buf, kbuf, len))
        return -EFAULT;

    *ppos += len;
    return len;
}

static const struct file_operations dt_sensor_fops = {
    .owner = THIS_MODULE,
    .open  = dt_sensor_open,
    .read  = dt_sensor_read,
};

/**
 * @brief Device Tree match table
 */
static const struct of_device_id dt_sensor_of_match[] = {
    { .compatible = "packt,custom-sensor", .data = (void *)1 },
    { .compatible = "packt,custom-sensor-v2", .data = (void *)2 },
    { /* Sentinel */ }
};
MODULE_DEVICE_TABLE(of, dt_sensor_of_match);

static int dt_sensor_probe(struct platform_device *pdev)
{
    struct dt_sensor_priv *priv;
    const struct of_device_id *match;
    int ret;

    dev_info(&pdev->dev, "probe() invoked for device '%s'\n", pdev->name);

    priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
    if (!priv)
        return -ENOMEM;

    priv->pdev = pdev;
    platform_set_drvdata(pdev, priv);

    /* 1. Check OF matching table if available */
    match = of_match_device(dt_sensor_of_match, &pdev->dev);
    if (match) {
        priv->matched_compatible = match->compatible;
        dev_info(&pdev->dev, "Matched OF compatible string: '%s'\n", match->compatible);
    } else {
        priv->matched_compatible = "packt,custom-sensor (emulated fwnode)";
    }

    /* 2. Read DT properties using the unified device_property API */
    ret = device_property_read_u32(&pdev->dev, "clock-frequency", &priv->clock_freq_hz);
    if (ret) {
        dev_warn(&pdev->dev, "Property 'clock-frequency' not found, defaulting to 100000 Hz\n");
        priv->clock_freq_hz = 100000;
    } else {
        dev_info(&pdev->dev, "Parsed 'clock-frequency': %u Hz\n", priv->clock_freq_hz);
    }

    ret = device_property_read_u32(&pdev->dev, "sampling-rate", &priv->sampling_rate_hz);
    if (ret) {
        dev_warn(&pdev->dev, "Property 'sampling-rate' not found, defaulting to 50 Hz\n");
        priv->sampling_rate_hz = 50;
    } else {
        dev_info(&pdev->dev, "Parsed 'sampling-rate': %u Hz\n", priv->sampling_rate_hz);
    }

    ret = device_property_read_string(&pdev->dev, "sensor-label", &priv->sensor_label);
    if (ret) {
        dev_warn(&pdev->dev, "Property 'sensor-label' not found\n");
        priv->sensor_label = "DEFAULT_SENSOR";
    } else {
        dev_info(&pdev->dev, "Parsed 'sensor-label': '%s'\n", priv->sensor_label);
    }

    priv->power_saving_enabled = device_property_read_bool(&pdev->dev, "power-saving");
    dev_info(&pdev->dev, "Parsed 'power-saving': %s\n", priv->power_saving_enabled ? "true" : "false");

    /* 3. Register character device for user-space verification */
    ret = alloc_chrdev_region(&priv->dev_num, 0, 1, CDEV_NAME);
    if (ret < 0)
        return ret;

    cdev_init(&priv->cdev, &dt_sensor_fops);
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

    dev_info(&pdev->dev, "/dev/%s ready for Device Tree property verification\n", CDEV_NAME);
    return 0;

destroy_class:
    class_destroy(priv->dev_class);
del_cdev:
    cdev_del(&priv->cdev);
unregister_chrdev:
    unregister_chrdev_region(priv->dev_num, 1);
    return ret;
}

static void dt_sensor_remove(struct platform_device *pdev)
{
    struct dt_sensor_priv *priv = platform_get_drvdata(pdev);

    dev_info(&pdev->dev, "remove() invoked\n");

    if (priv) {
        device_destroy(priv->dev_class, priv->dev_num);
        class_destroy(priv->dev_class);
        cdev_del(&priv->cdev);
        unregister_chrdev_region(priv->dev_num, 1);
    }
}

static struct platform_driver dt_sensor_driver = {
    .probe  = dt_sensor_probe,
    .remove = dt_sensor_remove,
    .driver = {
        .name           = DRIVER_NAME,
        .owner          = THIS_MODULE,
        .of_match_table = of_match_ptr(dt_sensor_of_match),
    },
};

/* Software Node Properties simulating a Device Tree node */
static const struct property_entry custom_sensor_props[] = {
    PROPERTY_ENTRY_STRING("compatible", "packt,custom-sensor"),
    PROPERTY_ENTRY_U32("clock-frequency", 400000),
    PROPERTY_ENTRY_U32("sampling-rate", 100),
    PROPERTY_ENTRY_STRING("sensor-label", "ACCEL_GYRO_V2"),
    PROPERTY_ENTRY_BOOL("power-saving"),
    { /* Sentinel */ }
};

static struct platform_device *demo_pdev;

static int __init dt_sensor_init(void)
{
    int ret;

    pr_info("Initializing Chapter 17 Device Tree Simulation Module\n");

    ret = platform_driver_register(&dt_sensor_driver);
    if (ret)
        return ret;

    /* Register platform device with attached simulated software node properties */
    demo_pdev = platform_device_alloc(DRIVER_NAME, 0);
    if (!demo_pdev) {
        platform_driver_unregister(&dt_sensor_driver);
        return -ENOMEM;
    }

    ret = device_create_managed_software_node(&demo_pdev->dev, custom_sensor_props, NULL);
    if (ret) {
        pr_warn("device_create_managed_software_node failed (%d)\n", ret);
    }

    ret = platform_device_add(demo_pdev);
    if (ret) {
        platform_device_put(demo_pdev);
        platform_driver_unregister(&dt_sensor_driver);
        return ret;
    }

    pr_info("Device Tree simulation driver loaded and device registered\n");
    return 0;
}

static void __exit dt_sensor_exit(void)
{
    pr_info("Unloading Chapter 17 Device Tree Simulation Module\n");

    if (demo_pdev)
        platform_device_unregister(demo_pdev);

    platform_driver_unregister(&dt_sensor_driver);
    pr_info("Clean teardown completed\n");
}

module_init(dt_sensor_init);
module_exit(dt_sensor_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Deepak Pandey");
MODULE_DESCRIPTION("Chapter 17: Device Tree Simulation & OF Property Parsing");
MODULE_VERSION("1.0");

