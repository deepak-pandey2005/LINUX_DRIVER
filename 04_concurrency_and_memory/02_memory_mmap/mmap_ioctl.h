/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file mmap_ioctl.h
 * @brief Chapter 12: IOCTL Command Definitions for Zero-Copy mmap
 * @author Deepak Pandey
 */

#ifndef MMAP_IOCTL_H
#define MMAP_IOCTL_H

#include <linux/ioctl.h>
#include <linux/types.h>

#define MMAP_MAGIC 'M'

/* IOCTL Commands */
#define MMAP_IOCTL_GET_BUF_SIZE    _IOR(MMAP_MAGIC, 1, __u32)
#define MMAP_IOCTL_KERNEL_FILL     _IOW(MMAP_MAGIC, 2, __u8)
#define MMAP_IOCTL_KERNEL_READ_STR _IOR(MMAP_MAGIC, 3, char[64])

#endif /* MMAP_IOCTL_H */
