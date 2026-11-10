# Chapter 18: GPIO Subsystem & Modern Descriptor-Based gpiod Consumer API

Welcome to Chapter 18 of the Linux Driver Development Curriculum. This lab covers modern General Purpose Input/Output (GPIO) handling in the Linux kernel using descriptor-based APIs (`struct gpio_desc`).

---

## 1. Overview & Learning Objectives

Historically, Linux drivers used integer numbers (e.g. `gpio_request(42, ...)`), which led to numbering conflicts, lack of type safety, and global namespace pollution across hardware architectures. Modern Linux kernels transitioned completely to the **descriptor-based `gpiod_*` framework**.

Key concepts mastered in this lab:
- Implementing a virtual GPIO controller using `struct gpio_chip` and `gpiochip_add_data()`.
- Modern `struct gpio_chip` callbacks: `direction_input`, `direction_output`, `get`, `get_direction`, and modern `int (*set)` (Linux 6.12+).
- Migrating from legacy integer GPIOs to `struct gpio_desc`.
- Using modern consumer functions: `gpiod_direction_input()`, `gpiod_direction_output()`, `gpiod_get_value()`, `gpiod_set_value()`.
- Exposing a user-space character device `/dev/virtual_gpio` with custom IOCTLs for real-time pin configuration and inspection.

---

## 2. Directory Structure

```text
03_gpio_driver/
├── Makefile             # Kbuild compilation and automated test rules
├── gpio_ioctl.h         # Shared IOCTL command definitions
├── gpio_driver_module.c # Driver implementing gpio_chip & descriptor consumer
├── test_client.c        # User-space verification suite
└── README.md            # This lab guide
```

---

## 3. Legacy vs Modern GPIO API

| Operation | Legacy API (Deprecated) | Modern Descriptor API (`<linux/gpio/consumer.h>`) |
| :--- | :--- | :--- |
| **Data Type** | `int gpio` | `struct gpio_desc *desc` |
| **Request** | `gpio_request(42, "led")` | `devm_gpiod_get(&pdev->dev, "led", GPIOD_OUT_LOW)` |
| **Direction**| `gpio_direction_output(42, 1)` | `gpiod_direction_output(desc, 1)` |
| **Write** | `gpio_set_value(42, 1)` | `gpiod_set_value(desc, 1)` |
| **Read** | `gpio_get_value(42)` | `gpiod_get_value(desc)` |
| **Interrupt**| `gpio_to_irq(42)` | `gpiod_to_irq(desc)` |

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
sudo insmod gpio_driver_module.ko
ls -la /dev/virtual_gpio
```

### Step 3: Run the Verification Suite
```bash
make test
```
The test client verifies:
- Discovery of the virtual GPIO controller and its 8 lines.
- Setting Pin 0 to OUTPUT HIGH (1) and reading back.
- Setting Pin 0 to LOW (0) and reading back.
- Walking an alternating bit pattern across all 8 pins (`0xAA`) and confirming the controller register bitmask.

### Step 4: Unload the Module
```bash
sudo make unload
```

---

## 5. Architectural Guide

For the full theoretical breakdown of GPIOLIB, active-low inversion flags, debounce filters, and GPIO character device ABI v2 (`/dev/gpiochipN`), see [docs/18_gpio_subsystem.md](file:///home/deepak/OPENSOURCE/LINUX_DRIVER/docs/18_gpio_subsystem.md).
