# Chapter 24: SPI Bus Driver Subsystem

## Overview
This lab demonstrates the architecture and programming model of the Linux **SPI (Serial Peripheral Interface)** subsystem. You will learn how the kernel manages master/slave communication, how host controllers (`struct spi_controller`) schedule synchronous and asynchronous transfers, how protocol drivers (`struct spi_driver`) bind to SPI devices, and how to execute full-duplex transfers using `spi_message`, `spi_transfer`, and `spi_sync`.

## Key Concepts
- **SPI Architecture**: Master-slave synchronous serial bus utilizing 4 lines: SCLK (Clock), MOSI (Master Out Slave In), MISO (Master In Slave Out), and CS/SS (Chip Select).
- **SPI Modes (0, 1, 2, 3)**: Defined by Clock Polarity (`CPOL`) and Clock Phase (`CPHA`).
- **`struct spi_controller`**: Kernel abstraction representing the SPI bus master controller with callback `transfer_one`.
- **`struct spi_driver`**: Protocol driver binding to SPI client devices matching `struct spi_device_id`.
- **`spi_transfer` & `spi_message`**: Fundamental units of transaction in the SPI subsystem, supporting full-duplex simultaneous send and receive.
- **Convenience Helpers**: `spi_write_then_read()` and `spi_sync_transfer()`.
- **Sysfs & Chardev Telemetry**: Tracking transfer metrics, clock rate, and mode settings via `/sys/class/mock_spi_class/spi_sensor/spi_stats`.

## Building and Running

### 1. Build Module and Client
```bash
make
```

### 2. Load the Module
```bash
sudo insmod spi_driver_module.ko
dmesg | tail -n 15
```

### 3. Grant Permissions to Device Node
```bash
sudo chmod 666 /dev/spi_sensor
```

### 4. Run the Verification Client
```bash
./test_client
```

### 5. Inspect Sysfs Attributes Directly
```bash
cat /sys/class/mock_spi_class/spi_sensor/spi_stats
```

### 6. Clean Up
```bash
sudo rmmod spi_driver_module
make clean
```
