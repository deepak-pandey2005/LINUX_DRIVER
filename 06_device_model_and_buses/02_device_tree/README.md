# Chapter 17: Device Tree Simulation & OF Property Parsing (of_device_id, properties)

Welcome to Chapter 17 of the Linux Driver Development Curriculum. This lab covers the **Device Tree (DT)** and **Open Firmware (OF)** subsystem, which describes hardware topologies to the Linux kernel without requiring hardcoded C device declarations.

---

## 1. Overview & Learning Objectives

On modern embedded architectures (ARM, ARM64, RISC-V, PowerPC), bootloaders (U-Boot, Coreboot) pass a flattened Device Tree Blob (`.dtb`) compiled from source files (`.dts`/`.dtsi`) to the Linux kernel at boot time.

Key concepts mastered in this lab:
- Understanding Device Tree structure: nodes, properties, phandles, and addressing.
- Defining Open Firmware match tables with `struct of_device_id` and `MODULE_DEVICE_TABLE(of, ...)`.
- Using `of_match_device()` and `of_device_get_match_data()` to customize driver behavior based on the matched `.compatible` string.
- Cross-platform property extraction using `<linux/property.h>`:
  - `device_property_read_u32()` for numeric integers.
  - `device_property_read_string()` for character strings.
  - `device_property_read_bool()` for flag properties.
- Simulating Device Tree properties on x86/non-OF kernels using kernel software nodes (`device_create_managed_software_node`).
- Device Tree Overlay syntax (`custom_sensor.dts`) for runtime board hardware expansions.

---

## 2. Directory Structure

```text
02_device_tree/
├── Makefile             # Kbuild compilation and automated test rules
├── custom_sensor.dts    # Device Tree overlay source example
├── device_tree_module.c # Kernel driver parsing OF/software node properties
├── test_client.c        # User-space verification suite
└── README.md            # This lab guide
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
sudo insmod device_tree_module.ko
ls -la /dev/dt_sensor
```

### Step 3: Run the Verification Suite
```bash
make test
```
The test client verifies:
- Connection to `/dev/dt_sensor`.
- Successful parsing of the Device Tree compatible string.
- Reading `clock-frequency` (400,000 Hz), `sampling-rate` (100 Hz), `sensor-label` ("ACCEL_GYRO_V2"), and boolean `power-saving` status.

### Step 4: Unload the Module
```bash
sudo make unload
```

---

## 4. Architectural Guide

For the full theoretical breakdown of Flattened Device Trees (FDT), Device Tree Compiler (`dtc`), overlay merging, and OF memory address translation, see [docs/17_device_tree.md](file:///home/deepak/OPENSOURCE/LINUX_DRIVER/docs/17_device_tree.md).

