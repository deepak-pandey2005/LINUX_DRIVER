# Chapter 10: Sysfs & Kernel Objects (kobject, attributes, groups)

Welcome to Chapter 10 of the Linux Driver Development Curriculum. This lab covers the Linux **sysfs** pseudo-filesystem and the unified kernel object model (`kobject`).

---

## 1. Overview & Learning Objectives

Sysfs (`/sys`) is the standard modern Linux interface for exposing device telemetry, hardware properties, and configurable driver parameters.

Key concepts mastered in this lab:
- Creating custom sysfs directory nodes using `kobject_create_and_add()`.
- Exposing readable (`__ATTR_RO`), writable (`__ATTR_WO`), and read-write (`__ATTR_RW`) attributes.
- Atomic registration and cleanup of attribute groups using `struct attribute_group` and `sysfs_create_group()`.
- Safe string parsing and numeric conversions with `sysfs_streq()` and `kstrtouint()`.
- Formatting output safely using modern `sysfs_emit()`.
- The Linux "one value per file" design contract.
- Thread-safe state mutation using mutexes.

---

## 2. Directory Structure

```text
03_sysfs/
├── Makefile          # Kbuild compilation and automated test rules
├── sysfs_module.c    # Kernel module implementing sysfs attributes
├── test_client.c     # User-space verification suite
└── README.md         # This lab guide
```

---

## 3. Sysfs Attributes Implemented

All attributes reside under `/sys/kernel/telemetry_subsystem/`:

| Attribute | Permissions | Type | Description |
| :--- | :---: | :---: | :--- |
| `device_mode` | 0644 (RW) | String | Active operational mode: `LOW_POWER`, `NORMAL`, or `OVERCLOCK` |
| `baud_rate` | 0644 (RW) | Integer | Configured baud rate (9600, 19200, 38400, 57600, 115200) |
| `uptime_seconds` | 0444 (RO) | Integer | Driver uptime in seconds since initialization |
| `stats` | 0444 (RO) | String | Formatted metrics summary (reads, writes, errors, active state) |
| `reset` | 0200 (WO) | Trigger | Writing `1` resets metrics and restores defaults |

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
sudo insmod sysfs_module.ko
ls -la /sys/kernel/telemetry_subsystem/
```

### Step 3: Run the User-Space Test Suite
```bash
make test
```
The test client verifies:
- Presence of all sysfs nodes.
- Initial reading of telemetry and device modes.
- Modifying modes and baud rates.
- Rejection of invalid inputs (`-EINVAL`).
- Triggering write-only reset action and confirming restoration.

### Step 4: Manual Command-Line Interaction
```bash
# Read active mode
cat /sys/kernel/telemetry_subsystem/device_mode

# Switch to OVERCLOCK mode
echo "OVERCLOCK" | sudo tee /sys/kernel/telemetry_subsystem/device_mode

# Test rejection of bad mode
echo "TURBO" | sudo tee /sys/kernel/telemetry_subsystem/device_mode

# Read telemetry statistics
cat /sys/kernel/telemetry_subsystem/stats

# Reset metrics
echo "1" | sudo tee /sys/kernel/telemetry_subsystem/reset
```

### Step 5: Unload the Module
```bash
sudo make unload
```

---

## 5. Architectural Guide

For the full theoretical breakdown of `struct kobject`, reference counting (`kref`), sysfs vs procfs design principles, and uevents, see [docs/10_sysfs_and_kobjects.md](file:///home/deepak/OPENSOURCE/LINUX_DRIVER/docs/10_sysfs_and_kobjects.md).

