# Chapter 25: Industrial I/O (IIO) Subsystem

## Overview
This lab covers the architecture and implementation of the Linux **Industrial I/O (IIO)** subsystem. Designed specifically for Analog-to-Digital Converters (ADCs), Digital-to-Analog Converters (DACs), Accelerometers, Gyroscopes, IMUs, Pressure sensors, and Temperature sensors, IIO bridges the gap between hardware sensors and user-space analytical applications (such as `libiio`).

## Key Concepts
- **`struct iio_dev`**: Core kernel data structure representing an IIO device. Allocated with `devm_iio_device_alloc()` and registered via `devm_iio_device_register()`.
- **Channel Specifications (`struct iio_chan_spec`)**:
  - `type`: Physical measurement type (`IIO_VOLTAGE`, `IIO_TEMP`, `IIO_ACCEL`, `IIO_PRESSURE`).
  - `info_mask_separate`: Attributes unique to this channel (e.g., `BIT(IIO_CHAN_INFO_RAW)`).
  - `info_mask_shared_by_type`: Attributes shared across channels of the same type (e.g., `BIT(IIO_CHAN_INFO_SCALE)`).
  - Modifiers: Multi-axis measurements (`IIO_MOD_X`, `IIO_MOD_Y`, `IIO_MOD_Z`).
- **Callbacks (`struct iio_info`)**:
  - `.read_raw`: Handles user-space requests for raw ADC values, scale factors, and offsets.
  - `.write_raw`: Handles calibration, offset, or sampling rate adjustments.
- **Sysfs Standard Interface**:
  - Raw counts: `in_voltage0_raw`, `in_temp_raw`, `in_accel_x_raw`
  - Engineering conversion scale: `in_voltage0_scale`, `in_accel_scale`
  - Formula: $\text{Value} = (\text{Raw} + \text{Offset}) \times \text{Scale}$

## Building and Running

### 1. Build Module and Client
```bash
make
```

### 2. Load the Module
```bash
sudo insmod iio_sensor_module.ko
dmesg | tail -n 10
```

### 3. Inspect Sysfs Directory
```bash
ls -l /sys/bus/iio/devices/iio:device*/
```

### 4. Run the Verification Client
```bash
./test_client
```

### 5. Clean Up
```bash
sudo rmmod iio_sensor_module
make clean
```
