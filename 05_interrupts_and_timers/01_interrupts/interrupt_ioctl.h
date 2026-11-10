/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file interrupt_ioctl.h
 * @brief Chapter 13: IOCTL Command Definitions for Interrupt Subsystem
 * @author Deepak Pandey
 */

#ifndef INTERRUPT_IOCTL_H
#define INTERRUPT_IOCTL_H

#include <linux/ioctl.h>
#include <linux/types.h>

#define IRQ_DEMO_MAGIC 'I'

struct irq_stats {
    __u32 irq_num;
    __u32 is_hardware_registered;
    __u64 total_interrupts;
    __u64 last_interrupt_ns;
    __u32 last_cpu;
};

/* IOCTL Commands */
#define IRQ_IOCTL_TRIGGER_SOFTWARE _IO(IRQ_DEMO_MAGIC, 1)
#define IRQ_IOCTL_GET_STATS        _IOR(IRQ_DEMO_MAGIC, 2, struct irq_stats)
#define IRQ_IOCTL_RESET_STATS      _IO(IRQ_DEMO_MAGIC, 3)

#endif /* INTERRUPT_IOCTL_H */

