# Chapter 32: Real-Time Clock (RTC) Subsystem

## 1. Architectural Overview of the Linux RTC Framework

A **Real-Time Clock (RTC)** is a battery-backed peripheral responsible for keeping accurate calendar time (Year, Month, Day, Hours, Minutes, Seconds) across system reboots, hardware resets, and deep power-down states. Unlike high-resolution CPU timestamp counters (TSC) or kernel tick timers (`jiffies`) which stop when power is disconnected, RTC hardware runs on low-power 32.768 kHz quartz crystal oscillators energized by coin-cell batteries (e.g., CR2032) or supercapacitors.

The Linux kernel RTC framework (`drivers/rtc/`) provides a unified, hardware-agnostic abstraction layer:

```
+-------------------------------------------------------------------------+
|                        User Space Applications                          |
|         (hwclock, systemd-timedated, chrony, /dev/rtcX, sysfs)          |
+-------------------------------------------------------------------------+
                                     |
                                     v
+-------------------------------------------------------------------------+
|                       Linux Kernel RTC Core Subsystem                   |
|     - Char Device Driver Node Management (/dev/rtc0, /dev/rtc1...)      |
|     - Sysfs Interface (/sys/class/rtc/rtcX/)                            |
|     - In-Kernel RTC Timers, Alarms, and Timerqueues                     |
|     - Wall Clock Synchronization (update_persistent_clock64)            |
+-------------------------------------------------------------------------+
                                     |
                                     v
+-------------------------------------------------------------------------+
|                       struct rtc_class_ops                              |
|       .read_time(dev, tm)         .set_time(dev, tm)                    |
|       .read_alarm(dev, alrm)      .set_alarm(dev, alrm)                 |
|       .alarm_irq_enable(dev, en)  .proc(dev, seq_file)                  |
+-------------------------------------------------------------------------+
                                     |
                                     v
+-------------------------------------------------------------------------+
|                    Hardware Bus Interface & Drivers                     |
|      - I2C RTCs (DS1307, PCF8563, RV3028)                               |
|      - SPI RTCs (MCP79410, DS3234)                                      |
|      - On-SoC Integrated RTCs (ARM AMBA PL031, STM32 RTC, x86 CMOS)     |
+-------------------------------------------------------------------------+
```

---

## 2. Core Data Structures and Operations

### 2.1 Calendar Time Representation: `struct rtc_time`
The Linux RTC subsystem defines calendar time in `<linux/rtc.h>` matching standard POSIX `struct tm` conventions:

```c
struct rtc_time {
    int tm_sec;   /* Seconds [0..59, or 60 for leap seconds] */
    int tm_min;   /* Minutes [0..59] */
    int tm_hour;  /* Hours [0..23] */
    int tm_mday;  /* Day of the month [1..31] */
    int tm_mon;   /* Month [0..11] (0 = January) */
    int tm_year;  /* Years since 1900 (e.g., 126 for 2026) */
    int tm_wday;  /* Day of the week [0..6] (0 = Sunday) */
    int tm_yday;  /* Day of the year [0..365] */
    int tm_isdst; /* Daylight Saving Time flag */
};
```

### 2.2 Alarm Data Structure: `struct rtc_wkalrm`
```c
struct rtc_wkalrm {
    unsigned char enabled; /* 1 = alarm interrupt enabled, 0 = disabled */
    unsigned char pending; /* 1 = alarm matched and interrupt pending */
    struct rtc_time time;  /* Programmed match timestamp */
};
```

### 2.3 Driver Callbacks: `struct rtc_class_ops`
Drivers populate `struct rtc_class_ops` to link hardware-specific bus transactions with kernel core routines:

```c
static const struct rtc_class_ops my_rtc_ops = {
    .read_time        = my_read_time,
    .set_time         = my_set_time,
    .read_alarm       = my_read_alarm,
    .set_alarm        = my_set_alarm,
    .alarm_irq_enable = my_alarm_irq_enable,
};
```

---

## 3. Registering RTC Devices with Modern Kernel APIs

In modern Linux kernels (Linux 5.x through 7.x), RTC drivers use managed resource allocators:

```c
static int my_rtc_probe(struct platform_device *pdev)
{
    struct my_rtc_priv *priv;
    int ret;

    priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
    if (!priv)
        return -ENOMEM;

    /* 1. Allocate RTC device wrapper */
    priv->rtc = devm_rtc_allocate_device(&pdev->dev);
    if (IS_ERR(priv->rtc))
        return PTR_ERR(priv->rtc);

    /* 2. Configure operations and supported date range */
    priv->rtc->ops = &my_rtc_ops;
    priv->rtc->range_min = RTC_TIMESTAMP_BEGIN_2000;
    priv->rtc->range_max = RTC_TIMESTAMP_END_2099;

    /* 3. Register wakeup capability for alarm wakeups */
    device_init_wakeup(&pdev->dev, true);

    /* 4. Register RTC with kernel core (creates /dev/rtcX and sysfs entries) */
    ret = devm_rtc_register_device(priv->rtc);
    if (ret < 0)
        return ret;

    return 0;
}
```

---

## 4. Hardware Interrupts and Alarm Wakeups

When a programmed hardware alarm matches the real-time clock counter:
1. The hardware asserts an active-low or level interrupt line connected to the SoC GPIO or interrupt controller.
2. The driver's interrupt handler verifies the alarm match register and invokes:
   ```c
   rtc_update_irq(priv->rtc, 1, RTC_IRQF | RTC_AF);
   ```
3. `rtc_update_irq()` handles:
   - Signaling waiting user space processes blocked in `read()` or `poll()`/`select()` on `/dev/rtcX`.
   - Propagating kernel-internal timer callbacks.
   - Waking the system if the device was in S3 Suspend-to-RAM.

```
+------------------------------------------------------------------------+
|                          Hardware Alarm Match                          |
|             (Simulated Timer or Physical Comparator Asserted)          |
+------------------------------------------------------------------------+
                                   |
                                   v
+------------------------------------------------------------------------+
|                      Driver ISR / Timer Callback                       |
|               priv->alarm_pending = true;                              |
|               rtc_update_irq(priv->rtc, 1, RTC_IRQF | RTC_AF);         |
+------------------------------------------------------------------------+
                                   |
                                   v
+------------------------------------------------------------------------+
|                        Kernel RTC Core Layer                           |
|       - Unblocks waiting processes in read() / select()                |
|       - Updates sysfs /sys/class/rtc/rtcX/wakealarm status             |
|       - Wakes system from S3 / S4 power states                         |
+------------------------------------------------------------------------+
```

---

## 5. Binary Coded Decimal (BCD) vs Binary Encoding

Many physical RTC chips (such as Maxim DS1307, NXP PCF8563, and Microchip MCP79410) store date and time registers in **Binary Coded Decimal (BCD)** to facilitate simple 7-segment display decoding in early electronic hardware. The kernel provides standardized conversion macros in `<linux/bcd.h>`:

```c
#include <linux/bcd.h>

/* Reading from hardware registers */
tm->tm_sec  = bcd2bin(reg_sec & 0x7F);
tm->tm_min  = bcd2bin(reg_min & 0x7F);
tm->tm_hour = bcd2bin(reg_hour & 0x3F);

/* Writing to hardware registers */
reg_sec  = bin2bcd(tm->tm_sec);
reg_min  = bin2bcd(tm->tm_min);
reg_hour = bin2bcd(tm->tm_hour);
```

---

## 6. User Space Interaction and Verification

User applications interact with RTC devices through `/dev/rtcX` using standard IOCTLs defined in `<linux/rtc.h>`:

| IOCTL Command | Description | Argument Type |
|:---|:---|:---|
| `RTC_RD_TIME` | Read current hardware calendar time | `struct rtc_time *` |
| `RTC_SET_TIME` | Set hardware calendar time | `const struct rtc_time *` |
| `RTC_ALM_READ` | Read programmed alarm time | `struct rtc_time *` |
| `RTC_ALM_SET` | Configure alarm match time | `const struct rtc_time *` |
| `RTC_AIE_ON` | Enable alarm interrupts | None (`unsigned long 0`) |
| `RTC_AIE_OFF` | Disable alarm interrupts | None (`unsigned long 0`) |
| `RTC_WKALM_RD` | Read alarm state & pending status | `struct rtc_wkalrm *` |
| `RTC_WKALM_SET` | Set alarm state & enable flag | `const struct rtc_wkalrm *` |

In addition, reading from an open file descriptor on `/dev/rtcX` blocks until an interrupt event fires:
```c
unsigned long data;
read(fd, &data, sizeof(data));
if (data & RTC_AF) {
    printf("Alarm interrupt event triggered!\n");
}
```
