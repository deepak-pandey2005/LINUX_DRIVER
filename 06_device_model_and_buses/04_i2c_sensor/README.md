# Chapter 19: I2C Client Drivers & Sensor Emulation

Welcome to Chapter 19 of the Linux Driver Development Curriculum. This lab covers the **Inter-Integrated Circuit (I2C)** and **System Management Bus (SMBus)** kernel subsystems.

---

## 1. Overview & Learning Objectives

The I2C bus is a synchronous, multi-master, multi-slave, packet-switched two-wire serial bus (SDA - Serial Data, SCL - Serial Clock) ubiquitous in consumer electronics, automotive ECUs, and server motherboards for sensors, EEPROMs, RTCs, and power managers.

Key concepts mastered in this lab:
- Architecture of the Linux I2C subsystem: **Adapters** (Masters/Controllers), **Algorithms**, and **Clients** (Slaves).
- Defining and registering a `struct i2c_driver`.
- Performing standard SMBus transactions: `i2c_smbus_read_byte_data()`, `i2c_smbus_read_word_swapped()`, and `i2c_smbus_write_byte_data()`.
- Parsing raw sensor registers (fixed-point 11-bit twos-complement) into physical units.
- Implementing an in-kernel virtual I2C adapter to enable full testing on any machine without hardware soldering.
- Dynamic client device instantiation with `i2c_new_client_device()`.

---

## 2. Directory Structure

```text
04_i2c_sensor/
├── Makefile            # Kbuild compilation and automated test rules
├── i2c_sensor_ioctl.h  # IOCTL definitions for temperature reads and configuration
├── i2c_sensor_module.c # Driver implementing I2C adapter & LM75 sensor client
├── test_client.c       # User-space verification suite
└── README.md           # This lab guide
```

---

## 3. Build, Load, and Test Instructions

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
sudo insmod i2c_sensor_module.ko
ls -la /dev/i2c_temp_sensor
```

### Step 3: Run the Verification Suite
```bash
make test
```
The test client verifies:
- Querying live temperature and verifying LM75 Chip ID (`0x75`).
- Writing to the I2C configuration register and reading it back over SMBus.
- Formatted telemetry inspection via `/dev/i2c_temp_sensor`.

### Step 4: Unload the Module
```bash
sudo make unload
```

---

## 4. Architectural Guide

For the full theoretical breakdown of I2C protocol arbitration, clock stretching, repeated starts, and SMBus vs raw I2C, see [docs/19_i2c_bus_driver.md](file:///home/deepak/OPENSOURCE/LINUX_DRIVER/docs/19_i2c_bus_driver.md).

