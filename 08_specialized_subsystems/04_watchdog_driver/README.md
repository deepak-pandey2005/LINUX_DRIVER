# Chapter 27: Watchdog Timer Subsystem

## Overview
This lab demonstrates the architecture and programming model of the **Linux Watchdog Subsystem**. Watchdog timers are critical safety mechanisms in embedded, industrial, and high-availability servers designed to recover systems from software deadlocks, kernel panics, or unresponsive user-space daemons by autonomously asserting a hardware reset.

## Key Concepts
- **`struct watchdog_device`**: Core abstraction representing a hardware or virtual watchdog timer.
- **`struct watchdog_ops`**: Operation callbacks (`.start`, `.stop`, `.ping`, `.set_timeout`, `.get_timeleft`).
- **Standard Character Device**: Exposed automatically by the watchdog core as `/dev/watchdog` (and `/dev/watchdog0`).
- **Heartbeat & Keepalive**: Periodically resetting the countdown timer before expiration via `write()` or `ioctl(WDIOC_KEEPALIVE)`.
- **Magic Close (`WDIOF_MAGICCLOSE`)**: The safety protocol requiring user processes to write `'V'` before closing `/dev/watchdog` to prevent accidental disarming if the monitoring daemon crashes.

## Building and Running

### 1. Build Module and Client
```bash
make
```

### 2. Load the Module
```bash
sudo insmod watchdog_driver_module.ko
dmesg | tail -n 10
```

### 3. Grant Permissions to Device Node
```bash
sudo chmod 666 /dev/watchdog
```

### 4. Run the Verification Client
```bash
./test_client
```

### 5. Inspect Sysfs Telemetry
```bash
cat /sys/devices/platform/mock_watchdog/wdt_stats
```

### 6. Clean Up
```bash
sudo rmmod watchdog_driver_module
make clean
```
