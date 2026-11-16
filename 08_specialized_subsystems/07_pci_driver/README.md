# Chapter 30: PCI / PCIe Subsystem & MSI-X Interrupts

## Overview
This lab covers the architecture and implementation of Linux **PCI (Peripheral Component Interconnect)** and **PCIe (PCI Express)** device drivers. You will learn how the kernel enumerates PCI buses, configures device identification tables (`struct pci_device_id`), maps hardware Base Address Registers (BARs) via `pci_iomap()`, enables bus mastering for high-speed DMA, and allocates Message Signaled Interrupts (MSI / MSI-X) using `pci_alloc_irq_vectors()`.

## Key Concepts
- **PCI Topology**: Host bridges, root complexes, PCIe switches, and endpoint devices.
- **`struct pci_driver`**:
  - `.name`: Driver identification.
  - `.id_table`: Array of supported `struct pci_device_id` pairs (Vendor ID, Device ID).
  - `.probe()`: Invoked upon device discovery.
  - `.remove()`: Invoked upon hot-unplug or driver unregistration.
- **Device Lifecycle Sequence**:
  1. `pci_enable_device(pdev)`: Activates memory/IO decoders and IRQ routing.
  2. `pci_set_master(pdev)`: Enables PCIe bus mastering for DMA engines.
  3. `pci_request_regions(pdev, name)`: Claims exclusive access to BAR regions.
  4. `pci_iomap(pdev, bar, maxlen)`: Maps physical PCI MMIO space into kernel address space.
- **Interrupts (INTx vs MSI vs MSI-X)**:
  - **Legacy INTx**: Level-triggered shared IRQ lines (pins INTA# through INTD#).
  - **MSI**: Message Signaled Interrupts; edge-triggered in-band memory writes (up to 32 vectors).
  - **MSI-X**: Extended MSI supporting up to 2048 distinct vectors with per-vector address/data pairs.
  - Allocated dynamically with `pci_alloc_irq_vectors(pdev, min, max, PCI_IRQ_ALL_TYPES)`.

## Building and Running

### 1. Build Module and Client
```bash
make
```

### 2. Load the Module
```bash
sudo insmod pci_driver_module.ko
dmesg | tail -n 10
```

### 3. Grant Permissions to Device Node
```bash
sudo chmod 666 /dev/pci_demo_dev
```

### 4. Run Verification Client
```bash
./test_client
```

### 5. Inspect Sysfs Telemetry
```bash
cat /sys/class/pci_demo_class/pci_demo_dev/pci_stats
```

### 6. Clean Up
```bash
sudo rmmod pci_driver_module
make clean
```
