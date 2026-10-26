/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file ioctl_commands.h
 * @brief Shared IOCTL Command Definitions and Data Structures.
 * @author Deepak Pandey
 *
 * This header is shared by BOTH the kernel driver and user-space applications.
 * It defines the Magic number, command macros (_IO, _IOR, _IOW, _IOWR),
 * and payload structs.
 */

#ifndef IOCTL_COMMANDS_H
#define IOCTL_COMMANDS_H

#ifdef __KERNEL__
#include <linux/ioctl.h>
#include <linux/types.h>
#else
#include <sys/ioctl.h>
#include <stdint.h>
#endif

// 8-bit Magic Number identifying our driver family
#define IOCTL_MAGIC 'k'

/**
 * @brief Device configuration payload (User -> Kernel)
 */
struct device_config {
    uint32_t baud_rate;
    uint32_t timeout_ms;
    uint8_t  operation_mode; // 0=Standard, 1=Turbo, 2=Low-Power
};

/**
 * @brief Device telemetry and status payload (Kernel -> User)
 */
struct device_status {
    uint32_t total_operations;
    uint32_t uptime_seconds;
    int32_t  temperature_mC; // Temperature in milli-Celsius (e.g. 36500 = 36.5 C)
    uint8_t  device_ready;   // 1=Ready, 0=Busy
};

/**
 * @brief Bidirectional exchange payload (User <-> Kernel)
 */
struct exchange_payload {
    char     message[64];
    uint32_t checksum;
};

/* =========================================================================
 * IOCTL Command Encodings
 * ========================================================================= */

// 1. Command with no data: Reset device state
#define IOCTL_RESET_DEVICE     _IO(IOCTL_MAGIC, 1)

// 2. Command writing configuration to driver: User -> Kernel
#define IOCTL_SET_CONFIG       _IOW(IOCTL_MAGIC, 2, struct device_config)

// 3. Command reading status from driver: Kernel -> User
#define IOCTL_GET_STATUS       _IOR(IOCTL_MAGIC, 3, struct device_status)

// 4. Bidirectional exchange: User <-> Kernel
#define IOCTL_TRANSFORM_DATA   _IOWR(IOCTL_MAGIC, 4, struct exchange_payload)

#endif /* IOCTL_COMMANDS_H */
