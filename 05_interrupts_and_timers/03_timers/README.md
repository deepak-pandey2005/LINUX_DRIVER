# Chapter 15: Kernel Timers, Jiffies & Delays (timer_list, hrtimer, delays)

Welcome to Chapter 15 of the Linux Driver Development Curriculum. This lab covers kernel timekeeping, jiffies, standard low-resolution timers, high-resolution timers (`hrtimer`), and delay mechanisms.

---

## 1. Overview & Learning Objectives

Drivers frequently need to measure elapsed time, schedule periodic monitoring heartbeats, poll hardware with timeouts, or delay bus transactions by precise microsecond or millisecond intervals.

Key concepts mastered in this lab:
- Understanding system ticks, `jiffies`, and the architecture-dependent tick rate `HZ`.
- Setting up and controlling standard low-resolution timers with `timer_setup()`, `mod_timer()`, and `timer_shutdown_sync()`.
- The softirq execution context of standard timers (`TIMER_SOFTIRQ`) and why sleeping is prohibited.
- Setting up modern high-resolution timers (`struct hrtimer`) with `hrtimer_setup()` and `hrtimer_start()` for sub-millisecond precision.
- Distinguishing busy-wait delays (`udelay`, `mdelay`) from sleepable delays (`usleep_range`, `msleep`).

---

## 2. Directory Structure

```text
03_timers/
├── Makefile          # Kbuild compilation and automated test rules
├── timer_ioctl.h     # IOCTL command definitions and statistics
├── timer_module.c    # Kernel driver implementing standard and high-resolution timers
├── test_client.c     # User-space verification suite
└── README.md         # This lab guide
```

---

## 3. Delays & Timers Compared

| Mechanism | Resolution | Context | CPU Behavior | Best Used For |
| :--- | :---: | :---: | :--- | :--- |
| **`udelay()` / `mdelay()`** | Micro/milli | Any (Atomic/IRQ) | Spins actively in tight loop | Sub-millisecond register setup in atomic context |
| **`msleep()`** | Milliseconds | Process context only | Context-switches process away | Sleeping >= 10 ms |
| **`usleep_range()`** | Microseconds | Process context only | Hrtimer-backed sleep | Precise 10 us – 20 ms sleeps with slack |
| **`struct timer_list`** | Jiffies (~1–10 ms)| `TIMER_SOFTIRQ` | Asynchronous callback | Periodic device heartbeats, connection watchdogs |
| **`struct hrtimer`** | Nanoseconds | Hard/soft IRQ | Asynchronous high-res callback | Audio frame clocks, high-speed sampling |

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
sudo insmod timer_module.ko
ls -la /dev/kernel_timers
```

### Step 3: Run the Verification Suite
```bash
make test
```
The test client verifies:
- Querying kernel `HZ` configuration and tick interval.
- Running a standard low-resolution periodic timer at 100 ms intervals and counting ticks.
- Running an ultra-precise high-resolution timer at 1000 us (1 ms) intervals and verifying rapid ticks.
- Clean and synchronous timer shutdown.

### Step 4: Unload the Module
```bash
sudo make unload
```

---

## 5. Architectural Guide

For the full theoretical breakdown of tickless kernels (`CONFIG_NO_HZ`), the timer wheel algorithm, clock sources, and high-resolution timekeeping architecture, see [docs/15_timers_and_delays.md](file:///home/deepak/OPENSOURCE/LINUX_DRIVER/docs/15_timers_and_delays.md).
