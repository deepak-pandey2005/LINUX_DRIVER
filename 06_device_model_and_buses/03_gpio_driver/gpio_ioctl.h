/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file gpio_ioctl.h
 * @brief Chapter 18: IOCTL Command Definitions for GPIO Subsystem
 * @author Deepak Pandey
 */

#ifndef GPIO_IOCTL_H
#define GPIO_IOCTL_H

#include <linux/ioctl.h>
#include <linux/types.h>

#define GPIO_DEMO_MAGIC 'G'

struct gpio_pin_req {
    __u32 pin_offset;  /* 0 to 7 */
    __u32 value;       /* 0 or 1 */
};

struct gpio_chip_info {
    __s32 base;
    __u32 ngpio;
    __u32 pin_values;      /* Bitmask of current values */
    __u32 pin_directions;  /* Bitmask: 1=IN, 0=OUT */
};

/* IOCTL Commands */
#define GPIO_IOCTL_SET_DIR_OUT   _IOW(GPIO_DEMO_MAGIC, 1, struct gpio_pin_req)
#define GPIO_IOCTL_SET_DIR_IN    _IOW(GPIO_DEMO_MAGIC, 2, __u32)
#define GPIO_IOCTL_WRITE_PIN     _IOW(GPIO_DEMO_MAGIC, 3, struct gpio_pin_req)
#define GPIO_IOCTL_READ_PIN      _IOWR(GPIO_DEMO_MAGIC, 4, struct gpio_pin_req)
#define GPIO_IOCTL_GET_CHIP_INFO _IOR(GPIO_DEMO_MAGIC, 5, struct gpio_chip_info)

#endif /* GPIO_IOCTL_H */

