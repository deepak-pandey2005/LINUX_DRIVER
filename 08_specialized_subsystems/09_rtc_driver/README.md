# Chapter 32: Real-Time Clock (RTC) Subsystem

## Overview
This lab covers the architecture and implementation of Linux **Real-Time Clock (RTC)** device drivers. Real-time clocks maintain accurate human-readable calendar time (year, month, day, hour, minute, second) independently of CPU power states, typically powered by an auxiliary lithium coin battery or supercapacitor when the main system is shut down.

In this chapter, you will learn how the Linux RTC framework organizes RTC drivers, how to implement `struct rtc_class_ops`, register an RTC device using `devm_rtc_allocate_device()` and `devm_rtc_register_device()`, configure alarm wakeups (`RTC_ALM_SET`), and signal hardware interrupt events to the kernel and user space via `rtc_update_irq()`.

## Key Concepts
- **Linux RTC Framework Architecture**:
  - **RTC Core**: Manages device nodes (`/dev/rtcX`), sysfs interfaces (`/sys/class/rtc/rtcX/`), in-kernel timerqueues, and synchronization with NTP and system wall clock.
  - **`struct rtc_device`**: Allocated with `devm_rtc_allocate_device()`, stores range limits (`range_min`, `range_max`) and driver operations.
  - **`struct rtc_class_ops`**:
    - `.read_time()`: Retrieves current calendar time into `struct rtc_time`.
    - `.set_time()`: Updates hardware RTC registers from user/NTP time.
    - `.read_alarm()` / `.set_alarm()`: Inspects and arms hardware alarm registers.
    - `.alarm_irq_enable()`: Enables or masks hardware alarm interrupt line.
- **Alarm Interrupt Delivery**:
  - Signaled into RTC core via `rtc_update_irq(rtc, 1, RTC_IRQF | RTC_AF)`.
  - Wakes up blocking user processes waiting in `read()` or `select()` on `/dev/rtcX`.
  - Supports system wakeup from S3 suspend-to-RAM via `device_init_wakeup(&pdev->dev, true)`.
- **Time Conversion Utilities**:
  - `rtc_tm_to_time64()`: Converts calendar `struct rtc_time` to 64-bit UTC epoch seconds.
  - `rtc_time64_to_tm()`: Converts 64-bit UTC epoch seconds to human-readable `struct rtc_time`.

## Building and Running

### 1. Build Module and Client
```bash
make
```

### 2. Load the Module
```bash
sudo insmod rtc_driver_module.ko
dmesg | tail -n 15
```

### 3. Grant Permissions to Device Node
```bash
# Check which rtc node was created (typically /dev/rtc1 if /dev/rtc0 is PC CMOS)
ls -la /dev/rtc*
sudo chmod 666 /dev/rtc1
```

### 4. Execute Verification Client
```bash
./test_client
```

### 5. Inspect Sysfs RTC Attributes
```bash
# General RTC time from kernel
cat /sys/class/rtc/rtc1/date
cat /sys/class/rtc/rtc1/time

# Custom driver telemetry
cat /sys/devices/platform/mock_rtc_driver/rtc_telemetry
```

### 6. Unload Module
```bash
sudo rmmod rtc_driver_module
dmesg | tail -n 10
```
