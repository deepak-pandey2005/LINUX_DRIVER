# Chapter 28: Linux PWM (Pulse Width Modulation) Subsystem

## Overview
This lab demonstrates the architecture and programming model of the **Linux PWM (Pulse Width Modulation)** subsystem. You will learn how the kernel manages periodic digital square waves, implements modern atomic state transitions via `struct pwm_ops` (`.apply` and `.get_state`), calculates frequencies and duty cycle ratios, and controls output through the sysfs interface (`/sys/class/pwm/pwmchipX/`).

## Key Concepts
- **Pulse Width Modulation (PWM)**: A technique for generating variable average voltage or motor speed using digital high/low pulses.
  - **Period**: Total cycle duration ($T$) measured in nanoseconds ($f = 1/T$).
  - **Duty Cycle**: High time duration ($t_{on}$) measured in nanoseconds ($\text{Duty} = \frac{t_{on}}{T} \times 100\%$).
  - **Polarity**: `PWM_POLARITY_NORMAL` (high during duty cycle) vs `PWM_POLARITY_INVERSED` (low during duty cycle).
- **`struct pwm_chip` & `struct pwm_ops`**:
  - Allocated with `devm_pwmchip_alloc()`.
  - Registered with `pwmchip_add()`.
  - Atomically updated via `chip->ops->apply(chip, pwm, state)`.
- **Sysfs Standard Interface (`/sys/class/pwm/pwmchipX/`)**:
  - `export`: Exports a channel (e.g. `echo 0 > export`).
  - `pwm0/period`: Nanoseconds duration of total period.
  - `pwm0/duty_cycle`: Nanoseconds duration of active high pulse.
  - `pwm0/enable`: `1` to activate waveform, `0` to halt.
  - `unexport`: Frees channel back to kernel control.

## Building and Running

### 1. Build Module and Client
```bash
make
```

### 2. Load the Module
```bash
sudo insmod pwm_driver_module.ko
dmesg | tail -n 10
```

### 3. Run the Verification Client
```bash
./test_client
```

### 4. Direct Manual Sysfs Testing
```bash
# Locate your pwmchip directory
cd /sys/class/pwm/pwmchip*

# Export channel 0
echo 0 | sudo tee export

# Configure 1 kHz frequency (1 ms period) with 50% duty cycle (0.5 ms)
echo 1000000 | sudo tee pwm0/period
echo 500000 | sudo tee pwm0/duty_cycle
echo 1 | sudo tee pwm0/enable

# View driver kernel logs
dmesg | tail -n 5

# Clean up
echo 0 | sudo tee pwm0/enable
echo 0 | sudo tee unexport
```

### 5. Clean Up
```bash
sudo rmmod pwm_driver_module
make clean
```
