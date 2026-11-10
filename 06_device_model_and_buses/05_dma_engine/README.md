# Chapter 20: DMA Engine & Zero-Copy Hardware Streaming (Coherent & Streaming DMA)

Welcome to Chapter 20 of the Linux Driver Development Curriculum. This lab covers **Direct Memory Access (DMA)**: the hardware mechanism that moves high-throughput bulk data directly between peripherals and physical RAM without consuming CPU instruction cycles.

---

## 1. Overview & Learning Objectives

At modern network speeds (100 Gbps), storage bandwidths (NVMe PCIe Gen 5), and video frame rates (4K 120 FPS), copying memory with the CPU via `memcpy()` or `copy_to_user()` brings processors to their knees. DMA hardware engines solve this bottleneck by taking over bus mastery.

Key concepts mastered in this lab:
- DMA addressing capabilities and masks (`dma_set_mask_and_coherent`).
- Virtual Addresses (VAs) vs Physical Addresses vs **Bus Addresses (`dma_addr_t`)**.
- **Coherent / Consistent DMA**: `dma_alloc_coherent()` and `dma_free_coherent()`.
- **Streaming DMA**: `dma_map_single()` and `dma_unmap_single()`.
- CPU Cache Invalidation & Flushing: `dma_sync_single_for_cpu()` and `dma_sync_single_for_device()`.
- Scatter-Gather architectures for non-contiguous physical pages.
- Exposing DMA telemetry to user space via `/dev/dma_engine_demo`.

---

## 2. Directory Structure

```text
05_dma_engine/
├── Makefile            # Kbuild compilation and automated test rules
├── dma_ioctl.h         # IOCTL command definitions and status struct
├── dma_engine_module.c # Driver implementing Coherent and Streaming DMA
├── test_client.c       # User-space verification suite
└── README.md           # This lab guide
```

---

## 3. Coherent vs Streaming DMA Compared

| Feature | Coherent DMA (`dma_alloc_coherent`) | Streaming DMA (`dma_map_single`) |
| :--- | :--- | :--- |
| **Allocation** | Kernel allocates page-aligned buffer | Driver provides existing `kmalloc` / page buffer |
| **Lifetime** | Long-lived (duration of driver operation)| Short-lived (single I/O transfer/packet) |
| **Cache Behavior** | Always synchronized in hardware | Requires software synchronization (`dma_sync_*`) |
| **Best Used For** | Circular descriptor rings, mailbox queues | High-bandwidth payload packets, video frames |

---

## 4. Build, Load, and Test Instructions

### Step 1: Compile the Module and Test Client
```bash
make
```

### Step 2: Load the Module
```bash
sudo make load
```
Or manually:
```bash
sudo insmod dma_engine_module.ko
ls -la /dev/dma_engine_demo
```

### Step 3: Run the Verification Suite
```bash
make test
```
The test client verifies:
- Discovery of physical Bus addresses for both Coherent and Streaming DMA buffers.
- Triggering high-speed DMA burst transfers between physical addresses.
- Cache synchronization verification.
- Diagnostic report generation.

### Step 4: Unload the Module
```bash
sudo make unload
```

---

## 5. Architectural Guide

For the full theoretical breakdown of IOMMUs (Intel VT-d, ARM SMMU), cache line bouncing, bounce buffers (`swiotlb`), and DMA scatter-gather lists, see [docs/20_dma_engine.md](file:///home/deepak/OPENSOURCE/LINUX_DRIVER/docs/20_dma_engine.md).

