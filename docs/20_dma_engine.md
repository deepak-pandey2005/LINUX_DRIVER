# Chapter 20: DMA Engine & Zero-Copy Hardware Streaming

Direct Memory Access (DMA) is the critical hardware capability allowing devices to directly read from and write to system RAM across buses (PCIe, AXI, AHB) without routing data through CPU registers.

---

## 1. Addressing Perspectives in Linux Drivers

A driver developer must manage three distinct addressing perspectives:

```mermaid
graph LR
    CPU["CPU Core"] -->|Virtual Address (0xFFFF...)| MMU["CPU MMU"]
    MMU -->|Physical Address (0x1000...)| RAM["System RAM"]
    Device["Peripheral (PCIe / DMA Controller)"] -->|Bus Address (dma_addr_t: 0x8000...)| IOMMU["IOMMU / Bus Controller"]
    IOMMU -->|Physical Address (0x1000...)| RAM
```

1. **Virtual Address (`void *`)**: The address used by CPU instructions in kernel or user space.
2. **Physical Address (`phys_addr_t`)**: The physical routing line on the DRAM bus.
3. **Bus Address (`dma_addr_t`)**: The address driven by the hardware peripheral onto the PCIe or system bus. When an IOMMU is present, the bus address maps through page tables to the physical address!

---

## 2. DMA Buffer Types

### 2.1 Coherent / Consistent DMA
Coherent memory guarantees that writes from the CPU are immediately visible to the device, and writes from the device are immediately visible to the CPU without manual cache invalidation:
```c
#include <linux/dma-mapping.h>

dma_addr_t dma_handle;
void *cpu_addr = dma_alloc_coherent(&pdev->dev, 4096, &dma_handle, GFP_KERNEL);

/* Program hardware controller with dma_handle */
writel(lower_32_bits(dma_handle), hw_reg_base + RING_ADDR_LOW);

/* Cleanup */
dma_free_coherent(&pdev->dev, 4096, cpu_addr, dma_handle);
```

### 2.2 Streaming DMA
For ephemeral transfers (such as transmitting a network packet or streaming a camera frame), allocating coherent memory is too slow. Instead, drivers map existing buffers with **Streaming DMA**:
```c
dma_addr_t dma_handle = dma_map_single(&pdev->dev, skb->data, skb->len, DMA_TO_DEVICE);
if (dma_mapping_error(&pdev->dev, dma_handle)) {
    /* Handle mapping failure */
}

/* Device completes transfer */
dma_unmap_single(&pdev->dev, dma_handle, skb->len, DMA_TO_DEVICE);
```

### 2.3 CPU Cache Flushing (`dma_sync_*`)
If a driver accesses a streaming buffer before unmapping it:
- **`dma_sync_single_for_cpu()`**: Flushes hardware updates to CPU cache and invalidates stale cache lines.
- **`dma_sync_single_for_device()`**: Writes dirty CPU cache lines back to RAM so the device sees new data.

