# Chapter 25: Industrial I/O (IIO) Subsystem

## 1. Subsystem Architecture

The **Industrial I/O (IIO)** subsystem was introduced to provide a unified kernel framework for analog-to-digital and digital-to-analog converters, environmental sensors (temperature, pressure, humidity), and inertial measurement units (accelerometers, gyroscopes, magnetometers).

Before IIO, sensor drivers were scattered haphazardly across `hwmon`, `input`, or custom character devices. IIO standardizes:
1. **Low-rate polled access**: Standardized sysfs attribute files (`in_*_raw`, `in_*_scale`, `in_*_offset`).
2. **High-rate streaming**: Ring buffers, hardware triggers, and character devices (`/dev/iio:deviceX`).
3. **Engineering unit conversions**: Translating raw integer LSB counts into standard SI units.

```
+--------------------------------------------------------------------+
|                      User Space Applications                       |
|           (libiio, IIO Oscilloscope, Sysfs, /dev/iio:deviceX)       |
+--------------------------------------------------------------------+
                                  |
                                  v
+--------------------------------------------------------------------+
|                     Linux IIO Core Subsystem                       |
|      - Channel Sysfs Generator (in_voltage0_raw, in_temp_scale)    |
|      - Buffer Engine & Trigger Management                          |
+--------------------------------------------------------------------+
                                  |
                                  v
+--------------------------------------------------------------------+
|                  IIO Driver (mock_iio_sensor)                      |
|      - struct iio_dev, struct iio_info (.read_raw, .write_raw)     |
|      - Channel array: struct iio_chan_spec[]                      |
+--------------------------------------------------------------------+
                                  |
                                  v
+--------------------------------------------------------------------+
|                     Physical Hardware / Bus                        |
|                     (I2C, SPI, ADC, Platform)                      |
+--------------------------------------------------------------------+
```

---

## 2. Channel Specification (`struct iio_chan_spec`)

Every analog or sensor input/output is declared via `struct iio_chan_spec`:

```c
static const struct iio_chan_spec channels[] = {
    {
        .type = IIO_VOLTAGE,
        .indexed = 1,
        .channel = 0,
        .info_mask_separate = BIT(IIO_CHAN_INFO_RAW),
        .info_mask_shared_by_type = BIT(IIO_CHAN_INFO_SCALE),
    },
    {
        .type = IIO_ACCEL,
        .modified = 1,
        .channel2 = IIO_MOD_X,
        .info_mask_separate = BIT(IIO_CHAN_INFO_RAW),
        .info_mask_shared_by_type = BIT(IIO_CHAN_INFO_SCALE),
    },
};
```

### Key Fields:
- **`type`**: `IIO_VOLTAGE`, `IIO_CURRENT`, `IIO_TEMP`, `IIO_ACCEL`, `IIO_PRESSURE`, `IIO_LIGHT`.
- **`info_mask_separate`**: Attributes generated specifically for this individual channel (e.g., `in_voltage0_raw`).
- **`info_mask_shared_by_type`**: Attributes shared across all channels of this type (e.g., `in_voltage_scale` applied to all voltage channels).
- **`modified` & `channel2`**: Used for multi-axis sensors (`IIO_MOD_X`, `IIO_MOD_Y`, `IIO_MOD_Z`).

---

## 3. Engineering Unit Standardization

In the Linux IIO subsystem, all physical quantities adhere to standardized base units:

| Measurement Type | Standard Kernel Unit | Sysfs Attribute Example |
|---|---|---|
| **Voltage** | Millivolts (mV) | `in_voltage0_raw`, `in_voltage0_scale` |
| **Current** | Milliamperes (mA) | `in_current0_raw` |
| **Temperature** | Millidegrees Celsius (m°C) | `in_temp_raw`, `in_temp_scale` |
| **Acceleration** | Meters per second squared ($m/s^2$) | `in_accel_x_raw`, `in_accel_scale` |
| **Angular Velocity** | Radians per second ($rad/s$) | `in_anglvel_x_raw` |
| **Pressure** | Kilopascals (kPa) | `in_pressure_raw` |

### Conversion Formula:
$$\text{Physical Value (SI)} = (\text{raw} + \text{offset}) \times \text{scale}$$

---

## 4. Driver Callback Implementation (`struct iio_info`)

The driver provides the dispatch operations table:

```c
static const struct iio_info sensor_iio_info = {
    .read_raw  = sensor_read_raw,
    .write_raw = sensor_write_raw,
};
```

When reading a channel:
- Return `IIO_VAL_INT`: Integer value returned in `*val`.
- Return `IIO_VAL_INT_PLUS_MICRO`: Floating-point format where `*val` is whole integer and `*val2` is micro-fraction ($10^{-6}$).
- Return `IIO_VAL_FRACTIONAL`: Fraction represented as `*val / *val2`.

---

## 5. Summary & Best Practices

- Always allocate IIO structures using `devm_iio_device_alloc(&pdev->dev, sizeof(*priv))` for managed lifecycle handling.
- Protect hardware read/write register sequences with a `mutex` inside `.read_raw` and `.write_raw`.
- Standardize on SI units to allow generic tools (e.g. `libiio`, Android HAL, ROS) to automatically interpret sensor data without custom vendor libraries.
