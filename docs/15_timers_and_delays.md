# Chapter 15: Kernel Timers, Jiffies & Delays (timer_list, hrtimer, delays)

Timekeeping is a critical pillar of operating system kernels. From hardware bus timing specifications (such as I2C clock stretching or PCIe reset pulses) to network packet timeouts and periodic device telemetry, Linux drivers rely on both low-resolution and high-resolution timer subsystems.

---

## 1. The Low-Resolution Timebase: `jiffies` and `HZ`

The kernel tracks system uptime via the global integer variable `jiffies` (and 64-bit `jiffies_64`).
- **`HZ`**: The architecture-defined frequency of the system timer interrupt. Common values:
  - `HZ = 100`: 1 tick = 10 ms (server throughput optimization).
  - `HZ = 250`: 1 tick = 4 ms (balanced desktop/server default).
  - `HZ = 1000`: 1 tick = 1 ms (low-latency audio / interactive systems).

### Converting Time to Jiffies:
```c
#include <linux/jiffies.h>

unsigned long delay = msecs_to_jiffies(100); /* Converts 100 ms to ticks */
unsigned long timeout = jiffies + delay;

if (time_after(jiffies, timeout)) {
    /* Timeout has expired */
}
```

---

## 2. Low-Resolution Timers: `struct timer_list`

Standard kernel timers are managed by the kernel **Timer Wheel** data structure:
```c
#include <linux/timer.h>

struct timer_list my_timer;

void my_timer_callback(struct timer_list *t)
{
    /* Runs in TIMER_SOFTIRQ context! DO NOT SLEEP! */
}

/* Initialization */
timer_setup(&my_timer, my_timer_callback, 0);

/* Start or rearm */
mod_timer(&my_timer, jiffies + msecs_to_jiffies(50));

/* Synchronous shutdown */
timer_shutdown_sync(&my_timer);
```

### Softirq Execution Context:
Timer callbacks run in **`TIMER_SOFTIRQ` context**:
- Interrupts are enabled, but process context is absent.
- Cannot call `msleep()`, `schedule()`, or block on a mutex.
- State shared between the timer callback and user operations must be protected with `spin_lock_irqsave()`.

---

## 3. High-Resolution Timers: `struct hrtimer`

For tasks requiring sub-millisecond or microsecond timing (such as periodic sampling, PWM signal generation, or real-time audio), standard jiffies-based timers are insufficiently granular.

The **hrtimer** subsystem bypasses the timer wheel and operates on hardware clock event devices (such as local APIC timers and HPET) with nanosecond precision:

```c
#include <linux/hrtimer.h>
#include <linux/ktime.h>

struct hrtimer my_hrtimer;

enum hrtimer_restart my_hrtimer_callback(struct hrtimer *timer)
{
    /* Nanosecond precision callback */
    ktime_t period = ktime_set(0, 500000); /* 500 microseconds */

    hrtimer_forward_now(timer, period);
    return HRTIMER_RESTART; /* Rearm periodically */
}

/* Setup */
hrtimer_setup(&my_hrtimer, my_hrtimer_callback, CLOCK_MONOTONIC, HRTIMER_MODE_REL);

/* Start */
hrtimer_start(&my_hrtimer, ktime_set(0, 500000), HRTIMER_MODE_REL);

/* Cancel */
hrtimer_cancel(&my_hrtimer);
```

---

## 4. Delay and Sleep Mechanisms

When a driver must wait for hardware to change states:

### 4.1 Busy-Wait (Atomic / Interrupt Context)
- `ndelay(ns)`: Nanosecond busy-wait loop.
- `udelay(us)`: Microsecond busy-wait loop (recommended for < 10 us).
- `mdelay(ms)`: Millisecond busy-wait loop. **Avoid in process context** because it wastes CPU cycles!

### 4.2 Sleepable Delays (Process Context Only)
- `usleep_range(min_us, max_us)`: Preferred for short sleeps (10 us to 20 ms). Backed by hrtimers with power-saving slack.
- `msleep(ms)`: Uninterruptible millisecond sleep (for >= 10 ms).
- `msleep_interruptible(ms)`: Sleep that awakens early if a signal is received.
