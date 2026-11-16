# Chapter 28: Linux PWM (Pulse Width Modulation) Subsystem

## 1. Architectural Overview of the Linux PWM Core

**Pulse Width Modulation (PWM)** is the primary digital technique used for:
- Backlight brightness control in displays.
- Fan speed regulation in cooling systems.
- Motor speed and servo angle positioning in robotics.
- Power switching converters (buck/boost regulators).

In the Linux kernel, the PWM core (`drivers/pwm/core.c`) abstracts disparate hardware timers, counter peripherals, and SoC waveform generators under a unified interface:

```
+--------------------------------------------------------------------+
|                    User Space Applications                         |
|             (Sysfs /sys/class/pwm/pwmchipX, systemd, GUI)          |
+--------------------------------------------------------------------+
                                  |
                                  v
+--------------------------------------------------------------------+
|                      Linux PWM Core Subsystem                      |
|      - Channel Export / Unexport Tracking                          |
|      - Atomic State Synchronization (pwm_apply_might_sleep)        |
+--------------------------------------------------------------------+
                                  |
                                  v
+--------------------------------------------------------------------+
|                  PWM Chip Driver (mock_pwm_chip)                   |
|      - struct pwm_chip, struct pwm_ops (.apply, .get_state)        |
|      - Channels: struct pwm_device *pwms                           |
+--------------------------------------------------------------------+
                                  |
                                  v
+--------------------------------------------------------------------+
|              Physical Hardware Timer / Counter Registers           |
+--------------------------------------------------------------------+
```

---

## 2. The Atomic State Paradigm (`struct pwm_state`)

Historically, older Linux kernels required separate, non-atomic function calls to modify period, duty cycle, and enable state (`pwm_config()`, `pwm_enable()`, `pwm_disable()`). This created glitching hazards where an intermediate invalid state (e.g., duty cycle > period) could trigger voltage spikes or motor stalls.

Modern Linux kernels enforce an **atomic update model**:

```c
struct pwm_state {
    u64 period;                  /* In nanoseconds */
    u64 duty_cycle;              /* In nanoseconds */
    enum pwm_polarity polarity;  /* NORMAL or INVERSED */
    bool enabled;                /* true = active, false = disabled */
    bool usage_power;
};
```

When user-space modifies sysfs files or a consumer driver calls `pwm_apply_might_sleep(pwm, &state)`, the PWM core calls the driver's `.apply()` callback once with all parameters combined.

---

## 3. Implementing Driver Callbacks (`struct pwm_ops`)

```c
static const struct pwm_ops my_pwm_ops = {
    .apply     = my_pwm_apply,
    .get_state = my_pwm_get_state,
};
```

### 3.1 `apply` Rules:
1. **Validation**: Check that `state->duty_cycle <= state->period`. If invalid, return `-EINVAL`.
2. **Frequency Calculation**:
   $$f = \frac{1}{\text{period (seconds)}} = \frac{10^9}{\text{period (ns)}}$$
3. **Duty Percentage**:
   $$\text{Duty \%} = \frac{\text{duty\_cycle}}{\text{period}} \times 100$$
4. **Hardware Programming**: If `state->enabled == false`, halt the timer clock to conserve power.

---

## 4. Sysfs User Interface

PWM channels are exposed through `/sys/class/pwm/`:

| Sysfs Node | Operation | Description |
|---|---|---|
| `pwmchipX/export` | Write channel index | Exports channel `N` to create `pwmN/` |
| `pwmchipX/unexport` | Write channel index | Destroys `pwmN/` and returns channel to kernel |
| `pwmchipX/pwmN/period` | Read / Write | Duration of whole cycle in nanoseconds |
| `pwmchipX/pwmN/duty_cycle` | Read / Write | Duration of high active state in nanoseconds |
| `pwmchipX/pwmN/polarity` | Read / Write | `normal` or `inversed` |
| `pwmchipX/pwmN/enable` | Read / Write | `1` to run, `0` to stop |

---

## 5. Summary & Best Practices

- Always allocate PWM chips using `devm_pwmchip_alloc(parent, npwm, sizeof_priv)`.
- Use nanosecond units exclusively throughout all PWM APIs.
- Protect internal channel register caching with a `mutex` or `spinlock` depending on whether hardware registers sleep.
