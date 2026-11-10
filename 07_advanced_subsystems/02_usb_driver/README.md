# Chapter 22: USB Device Driver Subsystem

## Overview
This lab demonstrates the architecture and programming model of the Linux USB Core Subsystem. You will explore USB driver registration (`struct usb_driver`), device ID table matching (`struct usb_device_id`), parsing interface descriptors and endpoints (Bulk IN/OUT), submitting asynchronous USB Request Blocks (URBs), handling transfer completion callbacks, and interacting with user-space through character device operations and `sysfs` telemetry.

## Key Concepts
- **`struct usb_driver`**: The USB driver registration structure with `.probe`, `.disconnect`, and `.id_table`.
- **`usb_device_id` & `MODULE_DEVICE_TABLE`**: Device matching criteria by Vendor ID (VID), Product ID (PID), or USB class/subclass.
- **Descriptors & Endpoints**: Navigating `usb_host_interface` and discovering Bulk IN and Bulk OUT pipes via `usb_endpoint_is_bulk_in/out`.
- **USB Request Blocks (URBs)**: The asynchronous I/O request unit of the USB stack (`usb_alloc_urb`, `usb_fill_bulk_urb`, `usb_submit_urb`).
- **Completion Callbacks**: URBs execute asynchronously; status and payload lengths are reported to the registered callback (`urb->complete`).
- **Simulated Loopback Engine**: When physical USB test hardware is absent, the driver simulates bidirectional bulk transfers, enabling safe validation in virtual or bare-metal environments.

## Building and Running

### 1. Build the Module and Client
```bash
make
```

### 2. Load the Module
```bash
sudo insmod usb_driver_module.ko
dmesg | tail -n 10
```

### 3. Verify Character Device and Permissions
```bash
ls -l /dev/usb_demo_dev
sudo chmod 666 /dev/usb_demo_dev
```

### 4. Execute Verification Test Client
```bash
./test_client
```

### 5. Inspect Sysfs Telemetry Directly
```bash
cat /sys/class/usb_demo_class/usb_demo_dev/usb_stats
```

### 6. Clean Up
```bash
sudo rmmod usb_driver_module
make clean
```
