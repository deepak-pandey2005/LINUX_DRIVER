# Chapter 16: Linux Device Model & Platform Drivers (probe, remove, resources)

Welcome to Chapter 16 of the Linux Driver Development Curriculum. This lab covers the **Linux Device Model (LDM)**: the unified framework representing buses, devices, drivers, and their dynamic binding via the **Platform Bus**.

---

## 1. Overview & Learning Objectives

In embedded systems and modern SoCs, many peripherals (UARTs, timers, GPIO banks, crypto engines, memory controllers) are directly mapped into physical address space rather than sitting on discoverable hardware buses like PCIe or USB. The Linux kernel represents these devices through the **Platform Bus** (`platform_bus_type`).

Key concepts mastered in this lab:
- The Linux Device Model triad: **Bus**, **Device**, and **Driver**.
- Defining and registering a `struct platform_driver`.
- Dynamic device-driver matching and the `probe()` callback.
- Modern `remove()` signature returning `void` (Linux 6.11+).
- Hardware resource extraction: `platform_get_resource()` (`IORESOURCE_MEM`) and `platform_get_irq()`.
- Device-managed resource allocations using `devm_kzalloc()`.
- Binding private driver state with `platform_set_drvdata()`.
- Verifying platform bus topology in `/sys/bus/platform/`.

---

## 2. Directory Structure

```text
01_platform_driver/
├── Makefile                # Kbuild compilation and automated test rules
├── platform_driver_module.c# Kernel driver implementing platform device & driver
├── test_client.c           # User-space verification suite
└── README.md               # This lab guide
```

---

## 3. Build, Load, and Test Instructions

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
sudo insmod platform_driver_module.ko
ls -la /dev/platform_demo
```

### Step 3: Run the Verification Suite
```bash
make test
```
The test client verifies:
- Platform device registration in `/sys/bus/platform/devices/`.
- Platform driver registration in `/sys/bus/platform/drivers/`.
- Device telemetry and resource mapping via `/dev/platform_demo`.

### Step 4: Unload the Module
```bash
sudo make unload
```

---

## 4. Architectural Guide

For the full theoretical breakdown of kobjects, bus types, device binding algorithms, and power management callbacks, see [docs/16_linux_device_model.md](file:///home/deepak/OPENSOURCE/LINUX_DRIVER/docs/16_linux_device_model.md).
