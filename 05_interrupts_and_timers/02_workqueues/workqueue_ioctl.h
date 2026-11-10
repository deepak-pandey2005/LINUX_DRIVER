/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file workqueue_ioctl.h
 * @brief Chapter 14: IOCTL Command Definitions for Workqueues Subsystem
 * @author Deepak Pandey
 */

#ifndef WORKQUEUE_IOCTL_H
#define WORKQUEUE_IOCTL_H

#include <linux/ioctl.h>
#include <linux/types.h>

#define WQ_MAGIC 'W'

struct wq_stats {
    __u32 default_work_count;
    __u32 custom_work_count;
    __u32 delayed_work_count;
    __u64 last_queue_time_ns;
    __u64 last_exec_time_ns;
    __u32 last_worker_pid;
};

/* IOCTL Commands */
#define WQ_IOCTL_TRIGGER_DEFAULT _IO(WQ_MAGIC, 1)
#define WQ_IOCTL_TRIGGER_CUSTOM  _IO(WQ_MAGIC, 2)
#define WQ_IOCTL_TRIGGER_DELAYED _IOW(WQ_MAGIC, 3, __u32) /* arg: delay in milliseconds */
#define WQ_IOCTL_CANCEL_DELAYED  _IO(WQ_MAGIC, 4)
#define WQ_IOCTL_GET_STATS       _IOR(WQ_MAGIC, 5, struct wq_stats)

#endif /* WORKQUEUE_IOCTL_H */

