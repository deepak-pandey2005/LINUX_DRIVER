# Chapter 31: Device Power Management & Runtime PM

## Overview
This lab demonstrates the architecture and implementation of modern Linux **Device Power Management (PM)** and **Runtime Power Management (Runtime PM)**. Modern embedded SoCs, mobile hardware, and green computing servers require devices to dynamically transition between active operation and ultra-low-power quiescent sleep states whenever idle.

This chapter implements a production-grade Linux platform driver demonstrating synchronous wakeups (`pm_runtime_get_sync()`), dynamic idle detection, configurable autosuspend timers (`pm_runtime_put_autosuspend()`), hardware clock and power rail simulation, character device integration, and sysfs telemetry.

## Key Concepts
- **System Sleep PM vs Runtime PM**:
  - **System Sleep PM** (`suspend`/`resume`): Coordinated system-wide transitions triggered by ACPI/OS sleep states (S3 Suspend-to-RAM, S4 Hibernation).
  - **Runtime PM** (`runtime_suspend`/`runtime_resume`/`runtime_idle`): Autonomous, per-device power gating when a peripheral is not actively serving requests.
- **`struct dev_pm_ops`**:
  - Macro helpers: `SYSTEM_SLEEP_PM_OPS` and `RUNTIME_PM_OPS`.
  - Bridges driver callbacks with the kernel power management core.
- **Reference Counting & State Machine**:
  - `pm_runtime_get_sync(dev)`: Increments device usage counter; synchronously powers up the device if currently suspended.
  - `pm_runtime_put_autosuspend(dev)`: Decrements device usage counter; marks last activity timestamp and arms the kernel autosuspend timer.
  - `pm_runtime_mark_last_busy(dev)`: Extends the idle timeout when active transactions occur.
  - `pm_runtime_allow(dev)` / `pm_runtime_forbid(dev)`: Configures `/sys/devices/.../power/control` for automated power management.
- **Hardware Simulation**:
  - Gating 100 MHz PLL clock oscillators and turning off 3.3V / 1.2V voltage regulator rails to minimize thermal dissipation.

## Building and Running

### 1. Build Module and Client
```bash
make
```

### 2. Load the Module
```bash
sudo insmod pm_driver_module.ko
dmesg | tail -n 15
```

### 3. Grant Permissions to Device Node
```bash
sudo chmod 666 /dev/pm_demo_dev
```

### 4. Execute Verification Client
```bash
./test_client
```

### 5. Inspect Sysfs Power Management Attributes
```bash
# Standard Linux PM runtime status
cat /sys/devices/platform/pm_demo_driver/power/runtime_status

# Runtime usage count
cat /sys/devices/platform/pm_demo_driver/power/runtime_usage

# Custom driver telemetry
cat /sys/devices/platform/pm_demo_driver/power_telemetry
```

### 6. Manually Toggle Force Active via Sysfs
```bash
# Force device awake
echo 1 | sudo tee /sys/devices/platform/pm_demo_driver/force_active
cat /sys/devices/platform/pm_demo_driver/power/runtime_status
# Output: active

# Release hold (autosuspend triggers after delay)
echo 0 | sudo tee /sys/devices/platform/pm_demo_driver/force_active
sleep 2.5
cat /sys/devices/platform/pm_demo_driver/power/runtime_status
# Output: suspended
```

### 7. Unload Module
```bash
sudo rmmod pm_driver_module
dmesg | tail -n 15
```
