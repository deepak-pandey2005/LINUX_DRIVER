# Chapter 31: Device Power Management & Runtime PM

## 1. Architectural Overview of Linux Power Management

Power dissipation is a critical design constraint across modern computing platforms—ranging from battery-constrained IoT sensors and mobile smartphones to hyperscale data centers. The Linux kernel power management framework provides two distinct, complementary subsystems:

1. **System Sleep Power Management (System PM)**:
   - Coordinated system-wide sleep transitions initiated by the OS or user space (e.g., `systemctl suspend`, writing to `/sys/power/state`).
   - Standard ACPI sleep states: **S0** (Working), **S3** (Suspend-to-RAM), **S4** (Hibernation / Suspend-to-Disk), and **S5** (Soft Off).
   - All drivers are systematically notified to save hardware state, disable interrupts, gate clocks, and shut down external power rails.

2. **Runtime Power Management (Runtime PM)**:
   - Per-device, dynamic, fine-grained power management executed while the operating system remains in the **S0** active state.
   - Peripherals are automatically powered down or placed into low-power retention states when idle, and powered up on-demand when user applications or kernel subsystems initiate I/O.
   - Governed by reference counting, activity timestamps, and autosuspend delay timers.

```
+-------------------------------------------------------------------------+
|                        User Space Applications                          |
|    (File Operations: open/read/write/ioctl, Sysfs /sys/power/state)     |
+-------------------------------------------------------------------------+
                                     |
                                     v
+-------------------------------------------------------------------------+
|                       Linux Kernel PM Core Subsystem                    |
|      - Device Power Management Core (drivers/base/power/)               |
|      - Runtime PM Workqueue & Timer Engine (rpm_suspend/resume)         |
|      - Usage Counter Management (dev->power.usage_count)                |
+-------------------------------------------------------------------------+
                                     |
                                     v
+-------------------------------------------------------------------------+
|                  Device Driver struct dev_pm_ops                        |
|   +---------------------------------+---------------------------------+ |
|   |         System Sleep Ops        |          Runtime PM Ops         | |
|   |  .suspend(dev), .resume(dev)    |  .runtime_suspend(dev)          | |
|   |  .freeze(dev),  .thaw(dev)      |  .runtime_resume(dev)           | |
|   |  .poweroff(dev),.restore(dev)   |  .runtime_idle(dev)             | |
|   +---------------------------------+---------------------------------+ |
+-------------------------------------------------------------------------+
                                     |
                                     v
+-------------------------------------------------------------------------+
|                   Physical / Simulated Hardware Hardware                |
|       - Voltage Regulators (3.3V, 1.2V Core VDD Rail Gating)            |
|       - Clock Generators (PLL Gating, Dynamic Frequency Scaling)        |
+-------------------------------------------------------------------------+
```

---

## 2. Runtime PM State Machine and Reference Counting

The Runtime PM core maintains a state machine for every registered device struct (`struct device`). The primary states are:
- `RPM_ACTIVE`: The device is fully powered, clock signals are oscillating, and hardware registers are accessible.
- `RPM_SUSPENDED`: The device is in a low-power sleep state; clocks are gated or power rails are off. Register accesses may hang or return bus aborts.
- `RPM_RESUMING`: Transitioning from suspended to active.
- `RPM_SUSPENDING`: Transitioning from active to suspended.

```
                  +--------------------------------+
                  |                                |
                  |          RPM_SUSPENDED         |
                  |                                |
                  +--------------------------------+
                     |                          ^
pm_runtime_get*()   |                          |  autosuspend timer expires
(usage_count > 0)    |                          |  (usage_count == 0)
                     v                          |
            +------------------+       +-------------------+
            |   RPM_RESUMING   |       |   RPM_SUSPENDING  |
            +------------------+       +-------------------+
                     |                          ^
      .runtime_resume() succeeds       .runtime_suspend() succeeds
                     |                          |
                     v                          |
                  +--------------------------------+
                  |                                |
                  |           RPM_ACTIVE           |
                  |                                |
                  +--------------------------------+
                     ^                          |
                     |  I/O activity occurs     |  pm_runtime_put*()
                     |  pm_runtime_mark_last_busy()
                     +--------------------------+
```

### Reference Counting Semantics
- Every device contains an atomic counter: `dev->power.usage_count`.
- When `usage_count > 0`, the device **must** remain in `RPM_ACTIVE`.
- When `usage_count == 0`, the device is considered idle and eligible for suspension.
- `pm_runtime_get_sync(dev)`:
  - Atomically increments `usage_count`.
  - If the device is currently `RPM_SUSPENDED`, it synchronously invokes the driver's `.runtime_resume()` callback and blocks until the device is ready.
- `pm_runtime_put_autosuspend(dev)`:
  - Atomically decrements `usage_count`.
  - Updates the last-busy timestamp and schedules an autosuspend timer via the kernel workqueue.
  - If no further activity occurs before the timer expires, `.runtime_suspend()` is invoked.

---

## 3. Implementation of `struct dev_pm_ops`

In modern Linux kernels, power management operations are defined via `struct dev_pm_ops` declared in `<linux/pm.h>` and `<linux/pm_runtime.h>`:

```c
#include <linux/pm.h>
#include <linux/pm_runtime.h>

static int my_runtime_suspend(struct device *dev)
{
    /* Gate clocks, shut down external voltage regulators */
    return 0;
}

static int my_runtime_resume(struct device *dev)
{
    /* Restore power rails, stabilize PLL clock, restore registers */
    return 0;
}

static int my_system_suspend(struct device *dev)
{
    /* Save device context before system enters S3/S4 */
    return 0;
}

static int my_system_resume(struct device *dev)
{
    /* Reinitialize hardware after system wakes from S3/S4 */
    return 0;
}

static const struct dev_pm_ops my_dev_pm_ops = {
    SYSTEM_SLEEP_PM_OPS(my_system_suspend, my_system_resume)
    RUNTIME_PM_OPS(my_runtime_suspend, my_runtime_resume, NULL)
};
```

---

## 4. Driver Initialization Sequence

A production platform or bus driver configures Runtime PM during `probe()`:

```c
static int my_probe(struct platform_device *pdev)
{
    struct device *dev = &pdev->dev;

    /* 1. Configure autosuspend delay in milliseconds */
    pm_runtime_set_autosuspend_delay(dev, 2000);

    /* 2. Enable autosuspend support */
    pm_runtime_use_autosuspend(dev);

    /* 3. Mark the device as active in the PM core */
    pm_runtime_set_active(dev);

    /* 4. Enable Runtime PM tracking */
    pm_runtime_enable(dev);

    /* 5. Set power/control sysfs property to 'auto' */
    pm_runtime_allow(dev);

    /* 6. Arm the autosuspend timer to allow idle sleep */
    pm_runtime_mark_last_busy(dev);
    pm_runtime_put_autosuspend(dev);

    return 0;
}
```

During driver removal:
```c
static void my_remove(struct platform_device *pdev)
{
    struct device *dev = &pdev->dev;

    /* Disable Runtime PM and cancel pending timers */
    pm_runtime_forbid(dev);
    pm_runtime_disable(dev);
    pm_runtime_dont_use_autosuspend(dev);
}
```

---

## 5. Locking Rules & Avoiding Deadlocks

Implementing Runtime PM requires strict adherence to kernel locking rules:
1. **Never call `pm_runtime_get_sync()` with a lock held that is also acquired inside `.runtime_resume()`**:
   - Doing so causes an immediate AB-BA deadlock.
   - Always acquire power references *before* acquiring internal driver mutexes or spinlocks.
2. **Context of Callbacks**:
   - By default, `.runtime_suspend()` and `.runtime_resume()` run in process context (can sleep, call `msleep()`, allocate memory).
   - If `pm_runtime_irq_safe(dev)` is called, the callbacks can run in atomic / hard IRQ context (cannot sleep, cannot call `mutex_lock`).
3. **Autosuspend in Fast Paths**:
   - In high-throughput read/write routines, avoid calling synchronous resume if the device is already active. Call `pm_runtime_mark_last_busy(dev)` to avoid timer churn.

---

## 6. User Space Sysfs Interface & Debugging

The Linux PM core exposes control and status attributes under `/sys/devices/.../power/`:

| Sysfs Attribute | Description | Values / Usage |
|:---|:---|:---|
| `runtime_status` | Current runtime state | `active`, `suspended`, `suspending`, `resuming` |
| `runtime_usage` | Reference counter | Integer reflecting active claims on hardware |
| `control` | Userspace policy | `auto` (allow runtime PM) or `on` (stay active) |
| `autosuspend_delay_ms` | Inactivity window | Delay in milliseconds before suspend |
| `runtime_active_time` | Total time active | Accumulated milliseconds in active state |
| `runtime_suspended_time` | Total time sleeping | Accumulated milliseconds in suspended state |

---

## 7. Verification Summary

In this lab, the verification client (`test_client`) verifies:
1. Hardware powers up automatically when `/dev/pm_demo_dev` is opened (`pm_runtime_get_sync`).
2. Telemetry reflects energized power rails (3.3V / 1.2V) and 100 MHz clock active.
3. Activity bursts refresh the autosuspend timestamp (`pm_runtime_mark_last_busy`).
4. Reconfiguring autosuspend delay via IOCTL dynamically shifts the sleep schedule.
5. Closing file handles drops `usage_count` to 0, triggering the kernel timer to invoke `.runtime_suspend()` and gate power rails.
