/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file dma_ioctl.h
 * @brief Chapter 20: IOCTL Command Definitions for DMA Subsystem
 * @author Deepak Pandey
 */

#ifndef DMA_IOCTL_H
#define DMA_IOCTL_H

#include <linux/ioctl.h>
#include <linux/types.h>

#define DMA_DEMO_MAGIC 'D'

struct dma_status_info {
    __u64 coherent_bus_addr;
    __u64 streaming_bus_addr;
    __u32 coherent_size;
    __u32 streaming_size;
    __u64 total_dma_transfers;
    __u64 last_transfer_time_ns;
};

/* IOCTL Commands */
#define DMA_IOCTL_START_TRANSFER _IO(DMA_DEMO_MAGIC, 1)
#define DMA_IOCTL_GET_STATUS     _IOR(DMA_DEMO_MAGIC, 2, struct dma_status_info)

#endif /* DMA_IOCTL_H */

