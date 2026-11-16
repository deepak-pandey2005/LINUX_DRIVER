# Chapter 30: PCI / PCIe Subsystem & MSI-X Interrupts

## 1. Architectural Overview of the Linux PCI Framework

The **PCI (Peripheral Component Interconnect)** and **PCIe (PCI Express)** bus specifications form the backbone of modern high-performance interconnects across computing hardware—connecting GPUs, NVMe SSDs, network interface cards (100GbE/400GbE), and FPGA accelerators.

The Linux kernel PCI core (`drivers/pci/`) provides:
- Automatic bus enumeration and recursive bridge traversal.
- Configuration space scanning (256 bytes for legacy PCI, 4096 bytes for PCIe).
- Resource assignment for Base Address Registers (BARs).
- MSI and MSI-X vector allocation.
- Advanced Error Reporting (AER) and PCIe Power Management (ASPM).

```
+--------------------------------------------------------------------+
|                    User Space Applications                         |
|             (lspci, sysfs /sys/bus/pci, /dev/pci_demo_dev)         |
+--------------------------------------------------------------------+
                                  |
                                  v
+--------------------------------------------------------------------+
|                       Linux PCI Core Subsystem                     |
|      - Root Complex & Bridge Enumeration                           |
|      - Resource Window Allocation (pci_request_regions)            |
|      - MSI / MSI-X Vector Management (pci_alloc_irq_vectors)       |
+--------------------------------------------------------------------+
                                  |
                                  v
+--------------------------------------------------------------------+
|                  PCI Device Driver (mock_pci_driver)               |
|      - struct pci_driver, struct pci_device_id                     |
|      - MMIO Access: pci_iomap, ioread32, iowrite32                 |
+--------------------------------------------------------------------+
                                  |
                                  v
+--------------------------------------------------------------------+
|               PCIe Physical Bus (Root Complex / Endpoint)          |
+--------------------------------------------------------------------+
```

---

## 2. Base Address Registers (BARs)

PCI devices declare their memory and I/O requirements using **Base Address Registers (BAR0 to BAR5)** in their 64-byte standard configuration space header.

### BAR Types:
1. **Memory BAR (32-bit or 64-bit)**: Maps registers or device onboard RAM into CPU physical memory space.
2. **I/O BAR**: Maps legacy x86 I/O port address space (deprecated on modern PCIe).
3. **Prefetchable vs Non-Prefetchable**: Non-prefetchable memory represents control registers with side effects upon read; prefetchable represents framebuffer or packet buffer memory without read side effects.

### Inspecting & Mapping BARs in Kernel Code:
```c
/* Claim BAR regions to prevent resource collisions */
pci_request_regions(pdev, "my_driver");

/* Extract physical memory boundaries */
resource_size_t start = pci_resource_start(pdev, 0);
resource_size_t len   = pci_resource_len(pdev, 0);

/* Map into kernel virtual address space */
void __iomem *mmio = pci_iomap(pdev, 0, 0);

/* Read / Write 32-bit registers */
u32 val = ioread32(mmio + REG_OFFSET);
iowrite32(0xCAFE, mmio + REG_OFFSET);
```

---

## 3. Interrupts: INTx vs MSI vs MSI-X

PCI has evolved through three generations of interrupt delivery:

| Characteristic | Legacy INTx | MSI (Message Signaled) | MSI-X (Extended MSI) |
|---|---|---|---|
| **Signaling** | Dedicated physical lines (INTA# - INTD#) | In-band PCIe memory write packet | In-band PCIe memory write packet |
| **Max Vectors** | 1 (shared across devices) | Up to 32 (power of 2) | Up to 2048 individual vectors |
| **Targeting** | Shared wire, must query status | Directly targets specific CPU core | Per-vector CPU core affinity |
| **Overhead** | High (shared ISR polling) | Low (no shared interrupt overhead)| Ultra-low (dedicated queue routing) |

### Unified Kernel Allocation API:
```c
int num_vecs = pci_alloc_irq_vectors(pdev, 1, 16, PCI_IRQ_ALL_TYPES);
if (num_vecs > 0) {
    int irq = pci_irq_vector(pdev, 0);
    request_irq(irq, pci_isr, 0, "pci_isr", dev);
}
```

---

## 4. The Standard Driver Probe Sequence

Every production PCIe driver must execute this canonical initialization sequence:
1. `pci_enable_device(pdev)`
2. `pci_set_master(pdev)`
3. `pci_request_regions(pdev, ...)`
4. `pci_iomap(pdev, bar, ...)`
5. `pci_alloc_irq_vectors(...)`
6. `request_irq(...)`

On unload (`remove`):
1. `free_irq(...)`
2. `pci_free_irq_vectors(pdev)`
3. `pci_iounmap(pdev, mmio)`
4. `pci_release_regions(pdev)`
5. `pci_clear_master(pdev)`
6. `pci_disable_device(pdev)`

---

## 5. Summary & Best Practices

- Always use `pci_alloc_irq_vectors()` rather than obsolete `pci_enable_msi()`.
- Use `ioread32()` / `iowrite32()` (or `memcpy_fromio()` / `memcpy_toio()`) for MMIO accesses to ensure memory-ordering barriers are respected.
- Call `pci_set_drvdata(pdev, priv)` and `pci_get_drvdata(pdev)` to link private driver state to the device.
