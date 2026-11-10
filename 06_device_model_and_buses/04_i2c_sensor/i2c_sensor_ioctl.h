/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file i2c_sensor_ioctl.h
 * @brief Chapter 19: IOCTL Command Definitions for I2C Sensor Driver
 * @author Deepak Pandey
 */

#ifndef I2C_SENSOR_IOCTL_H
#define I2C_SENSOR_IOCTL_H

#include <linux/ioctl.h>
#include <linux/types.h>

#define I2C_SENSOR_MAGIC 'S'

struct i2c_temp_data {
    __s32 temp_milli_celsius; /* e.g. 25500 for 25.50 C */
    __u8 chip_id;             /* Expected: 0x75 */
    __u8 config_reg;          /* Configuration register */
};

/* IOCTL Commands */
#define I2C_IOCTL_GET_TEMP     _IOR(I2C_SENSOR_MAGIC, 1, struct i2c_temp_data)
#define I2C_IOCTL_SET_CONFIG   _IOW(I2C_SENSOR_MAGIC, 2, __u8)

#endif /* I2C_SENSOR_IOCTL_H */

