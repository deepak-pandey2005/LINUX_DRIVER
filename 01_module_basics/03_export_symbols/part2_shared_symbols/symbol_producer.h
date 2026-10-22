// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file symbol_producer.h
 * @brief Header exporting functions and variables across distinct kernel modules.
 * @author Deepak Pandey
 */

#ifndef SYMBOL_PRODUCER_H
#define SYMBOL_PRODUCER_H

#include <linux/types.h>

/**
 * @brief Computes a simple Fletcher-style checksum for data verification.
 * @param buffer Pointer to data buffer in kernel memory.
 * @param len Length in bytes.
 * @return 32-bit checksum.
 */
u32 compute_device_checksum(const u8 *buffer, size_t len);

/**
 * @brief Global counter tracking system-wide driver events.
 */
extern int global_driver_event_count;

#endif /* SYMBOL_PRODUCER_H */
