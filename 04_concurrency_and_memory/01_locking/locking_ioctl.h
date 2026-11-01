/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file locking_ioctl.h
 * @brief Chapter 11: IOCTL Command Definitions for Concurrency Benchmarking
 * @author Deepak Pandey
 */

#ifndef LOCKING_IOCTL_H
#define LOCKING_IOCTL_H

#include <linux/ioctl.h>
#include <linux/types.h>

#define LOCKING_MAGIC_MAGIC 'L'

/**
 * Snapshot of counter results returned to user-space
 */
struct locking_results {
    __u64 unsafe_val;
    __u64 atomic_val;
    __u64 spinlock_val;
    __u64 mutex_val;
};

/* IOCTL Commands */
#define LOCKING_IOCTL_RESET       _IO(LOCKING_MAGIC_MAGIC, 0)
#define LOCKING_IOCTL_INC_UNSAFE  _IO(LOCKING_MAGIC_MAGIC, 1)
#define LOCKING_IOCTL_INC_ATOMIC  _IO(LOCKING_MAGIC_MAGIC, 2)
#define LOCKING_IOCTL_INC_SPIN    _IO(LOCKING_MAGIC_MAGIC, 3)
#define LOCKING_IOCTL_INC_MUTEX   _IO(LOCKING_MAGIC_MAGIC, 4)
#define LOCKING_IOCTL_GET_RESULTS _IOR(LOCKING_MAGIC_MAGIC, 5, struct locking_results)

#endif /* LOCKING_IOCTL_H */

