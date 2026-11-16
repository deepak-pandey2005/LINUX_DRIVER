// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file iio_sensor_module.c
 * @brief Chapter 25: Industrial I/O (IIO) Subsystem (iio_dev, iio_info, iio_chan_spec)
 * @author Deepak Pandey
 *
 * This module demonstrates:
 *  - Architecture of the Linux Industrial I/O (IIO) Subsystem.
 *  - Allocating and registering an IIO device (struct iio_dev, devm_iio_device_alloc).
 *  - Defining hardware channels using struct iio_chan_spec:
 *      * Analog Voltage ADC channel (IIO_VOLTAGE)
 *      * Die Temperature sensor channel (IIO_TEMP)
 *      * 3-Axis Accelerometer channels (IIO_ACCEL, IIO_MOD_X, Y, Z)
 *  - Implementing the struct iio_info callback interface:
 *      * .read_raw() for raw counts and engineering scale factors.
 *      * .write_raw() for user calibration and dynamic offset setting.
 *  - Exposing standardized sysfs interfaces under /sys/bus/iio/devices/iio:deviceX.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>
#include <linux/mutex.h>
#include <linux/random.h>

#define DRIVER_NAME "mock_iio_sensor"

/**
 * @brief Private driver state context
 */
struct mock_iio_priv {
    struct mutex lock;

    /* Simulated Hardware Sensor Registers */
    int raw_voltage;     /* Simulated 12-bit ADC count (0 - 4095) */
    int voltage_scale;   /* Microvolts per LSB */
    int raw_temp;        /* Millidegrees Celsius raw */
    int accel_x;         /* G-force raw count */
    int accel_y;
    int accel_z;
    int accel_scale;     /* Micro-G per LSB */
};

/**
 * @brief Channel specifications defining exposed sensor capabilities
 */
static const struct iio_chan_spec mock_iio_channels[] = {
    /* 1. Voltage Channel: in_voltage0_raw, in_voltage0_scale */
    {
        .type = IIO_VOLTAGE,
        .indexed = 1,
        .channel = 0,
        .info_mask_separate = BIT(IIO_CHAN_INFO_RAW),
        .info_mask_shared_by_type = BIT(IIO_CHAN_INFO_SCALE),
    },
    /* 2. Temperature Channel: in_temp_raw, in_temp_scale */
    {
        .type = IIO_TEMP,
        .indexed = 0,
        .info_mask_separate = BIT(IIO_CHAN_INFO_RAW) | BIT(IIO_CHAN_INFO_SCALE),
    },
    /* 3. 3-Axis Accelerometer: in_accel_x_raw, in_accel_y_raw, in_accel_z_raw */
    {
        .type = IIO_ACCEL,
        .modified = 1,
        .channel2 = IIO_MOD_X,
        .info_mask_separate = BIT(IIO_CHAN_INFO_RAW),
        .info_mask_shared_by_type = BIT(IIO_CHAN_INFO_SCALE),
    },
    {
        .type = IIO_ACCEL,
        .modified = 1,
        .channel2 = IIO_MOD_Y,
        .info_mask_separate = BIT(IIO_CHAN_INFO_RAW),
        .info_mask_shared_by_type = BIT(IIO_CHAN_INFO_SCALE),
    },
    {
        .type = IIO_ACCEL,
        .modified = 1,
        .channel2 = IIO_MOD_Z,
        .info_mask_separate = BIT(IIO_CHAN_INFO_RAW),
        .info_mask_shared_by_type = BIT(IIO_CHAN_INFO_SCALE),
    },
};

/**
 * @brief Callback invoked by IIO core when user-space queries a channel attribute
 */
static int mock_iio_read_raw(struct iio_dev *indio_dev,
                             struct iio_chan_spec const *chan,
                             int *val, int *val2, long mask)
{
    struct mock_iio_priv *priv = iio_priv(indio_dev);

    switch (mask) {
    case IIO_CHAN_INFO_RAW:
        mutex_lock(&priv->lock);
        switch (chan->type) {
        case IIO_VOLTAGE:
            /* Add small jitter to simulate active ADC conversions */
            *val = priv->raw_voltage + (get_random_u32() % 10) - 5;
            break;
        case IIO_TEMP:
            *val = priv->raw_temp;
            break;
        case IIO_ACCEL:
            switch (chan->channel2) {
            case IIO_MOD_X:
                *val = priv->accel_x;
                break;
            case IIO_MOD_Y:
                *val = priv->accel_y;
                break;
            case IIO_MOD_Z:
                *val = priv->accel_z;
                break;
            default:
                mutex_unlock(&priv->lock);
                return -EINVAL;
            }
            break;
        default:
            mutex_unlock(&priv->lock);
            return -EINVAL;
        }
        mutex_unlock(&priv->lock);
        return IIO_VAL_INT;

    case IIO_CHAN_INFO_SCALE:
        mutex_lock(&priv->lock);
        switch (chan->type) {
        case IIO_VOLTAGE:
            *val = 0;
            *val2 = priv->voltage_scale; /* Microvolts */
            mutex_unlock(&priv->lock);
            return IIO_VAL_INT_PLUS_MICRO;
        case IIO_TEMP:
            *val = 1000; /* Scale factor: millidegrees to degrees */
            mutex_unlock(&priv->lock);
            return IIO_VAL_INT;
        case IIO_ACCEL:
            *val = 0;
            *val2 = priv->accel_scale;
            mutex_unlock(&priv->lock);
            return IIO_VAL_INT_PLUS_MICRO;
        default:
            mutex_unlock(&priv->lock);
            return -EINVAL;
        }

    default:
        return -EINVAL;
    }
}

/**
 * @brief Callback invoked when user-space writes a new calibration factor
 */
static int mock_iio_write_raw(struct iio_dev *indio_dev,
                              struct iio_chan_spec const *chan,
                              int val, int val2, long mask)
{
    struct mock_iio_priv *priv = iio_priv(indio_dev);

    switch (mask) {
    case IIO_CHAN_INFO_RAW:
        mutex_lock(&priv->lock);
        if (chan->type == IIO_VOLTAGE)
            priv->raw_voltage = val;
        else if (chan->type == IIO_TEMP)
            priv->raw_temp = val;
        mutex_unlock(&priv->lock);
        return 0;

    case IIO_CHAN_INFO_SCALE:
        mutex_lock(&priv->lock);
        if (chan->type == IIO_VOLTAGE)
            priv->voltage_scale = val2;
        else if (chan->type == IIO_ACCEL)
            priv->accel_scale = val2;
        mutex_unlock(&priv->lock);
        return 0;

    default:
        return -EINVAL;
    }
}

static const struct iio_info mock_iio_info = {
    .read_raw  = mock_iio_read_raw,
    .write_raw = mock_iio_write_raw,
};

static int mock_iio_probe(struct platform_device *pdev)
{
    struct iio_dev *indio_dev;
    struct mock_iio_priv *priv;
    int ret;

    pr_info("Probing Chapter 25 IIO Subsystem Sensor\n");

    /* 1. Allocate IIO device structure with private state */
    indio_dev = devm_iio_device_alloc(&pdev->dev, sizeof(*priv));
    if (!indio_dev)
        return -ENOMEM;

    priv = iio_priv(indio_dev);
    mutex_init(&priv->lock);

    /* Initialize baseline sensor register values */
    priv->raw_voltage   = 2048; /* Mid-scale ~ 1.65V */
    priv->voltage_scale = 805;  /* 3300000 uV / 4096 counts = ~805.6 uV/LSB */
    priv->raw_temp      = 28500;/* 28.5 degrees Celsius */
    priv->accel_x       = 12;   /* Static orientation resting */
    priv->accel_y       = -8;
    priv->accel_z       = 1018; /* ~1G on Z axis */
    priv->accel_scale   = 9806; /* 1G = 9.80665 m/s^2 */

    /* 2. Configure IIO device properties */
    indio_dev->name         = "mock_iio_adc";
    indio_dev->modes        = INDIO_DIRECT_MODE;
    indio_dev->info         = &mock_iio_info;
    indio_dev->channels     = mock_iio_channels;
    indio_dev->num_channels = ARRAY_SIZE(mock_iio_channels);

    /* 3. Register IIO device with Linux core */
    ret = devm_iio_device_register(&pdev->dev, indio_dev);
    if (ret) {
        pr_err("devm_iio_device_register failed: %d\n", ret);
        return ret;
    }

    platform_set_drvdata(pdev, indio_dev);
    pr_info("IIO Sensor registered successfully! Name: %s (%s)\n",
            indio_dev->name, dev_name(&indio_dev->dev));

    return 0;
}

static void mock_iio_remove(struct platform_device *pdev)
{
    pr_info("Removing Chapter 25 IIO Subsystem Sensor\n");
}

static struct platform_driver mock_iio_platform_driver = {
    .driver = {
        .name = DRIVER_NAME,
        .owner = THIS_MODULE,
    },
    .probe  = mock_iio_probe,
    .remove = mock_iio_remove,
};

static struct platform_device *g_iio_pdev;

static int __init mock_iio_init(void)
{
    int ret;

    pr_info("Initializing Chapter 25 IIO Subsystem Driver\n");

    ret = platform_driver_register(&mock_iio_platform_driver);
    if (ret) {
        pr_err("platform_driver_register failed: %d\n", ret);
        return ret;
    }

    g_iio_pdev = platform_device_register_simple(DRIVER_NAME, 0, NULL, 0);
    if (IS_ERR(g_iio_pdev)) {
        ret = PTR_ERR(g_iio_pdev);
        platform_driver_unregister(&mock_iio_platform_driver);
        return ret;
    }

    return 0;
}

static void __exit mock_iio_exit(void)
{
    pr_info("Unloading Chapter 25 IIO Subsystem Driver\n");

    if (g_iio_pdev)
        platform_device_unregister(g_iio_pdev);

    platform_driver_unregister(&mock_iio_platform_driver);
    pr_info("IIO driver unloaded cleanly\n");
}

module_init(mock_iio_init);
module_exit(mock_iio_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Deepak Pandey");
MODULE_DESCRIPTION("Chapter 25: Industrial I/O (IIO) Subsystem & Multi-Channel Sensor Driver");
MODULE_VERSION("1.0");
