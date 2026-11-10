/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file timer_ioctl.h
 * @brief Chapter 15: IOCTL Command Definitions for Kernel Timers Subsystem
 * @author Deepak Pandey
 */

#ifndef TIMER_IOCTL_H
#define TIMER_IOCTL_H

#include <linux/ioctl.h>
#include <linux/types.h>

#define TIMER_DEMO_MAGIC 'T'

struct timer_stats {
    __u32 std_timer_ticks;
    __u32 hr_timer_ticks;
    __u64 last_std_timestamp_ns;
    __u64 last_hr_timestamp_ns;
    __u32 hz_rate;
};

/* IOCTL Commands */
#define TIMER_IOCTL_START_STD _IOW(TIMER_DEMO_MAGIC, 1, __u32) /* arg: period in ms */
#define TIMER_IOCTL_STOP_STD  _IO(TIMER_DEMO_MAGIC, 2)
#define TIMER_IOCTL_START_HR  _IOW(TIMER_DEMO_MAGIC, 3, __u32) /* arg: period in microseconds */
#define TIMER_IOCTL_STOP_HR   _IO(TIMER_DEMO_MAGIC, 4)
#define TIMER_IOCTL_GET_STATS _IOR(TIMER_DEMO_MAGIC, 5, struct timer_stats)

#endif /* TIMER_IOCTL_H */

