# Chapter 22: USB Device Driver Subsystem

## 1. Architectural Overview of the Linux USB Stack

The Universal Serial Bus (USB) is a master-slave, polled bus architecture where the Host Controller initiates all communication transactions with peripheral devices.

The Linux USB subsystem is organized into three primary layers:
1. **Host Controller Drivers (HCD)**: Hardware-specific controllers (e.g., xHCI for USB 3.0+, EHCI for USB 2.0, OHCI/UHCI for USB 1.1).
2. **USB Core (`drivers/usb/core/`)**: The protocol and routing engine that handles device enumeration, configuration selection, descriptor caching, power management, and URB dispatching.
3. **USB Device Drivers**: Function drivers (such as mass storage, USB-serial, audio, HID, or custom vendor drivers) that bind to specific interfaces on a USB device.

```
+-------------------------------------------------------------+
|                  User Space Applications                    |
|          (Character Device /dev/usb_demo_dev / Sysfs)       |
+-------------------------------------------------------------+
                               |
                               v
+-------------------------------------------------------------+
|              USB Device Driver (usb_driver_module)          |
|    - Endpoint Discovery (Bulk IN: 0x81, Bulk OUT: 0x02)      |
|    - URB Allocation & Submission (usb_submit_urb)           |
+-------------------------------------------------------------+
                               |
                               v
+-------------------------------------------------------------+
|                    Linux USB Core Subsystem                 |
|       - Device Enumeration & Identification (id_table)      |
|       - Bandwidth Management & Asynchronous Scheduling      |
+-------------------------------------------------------------+
                               |
                               v
+-------------------------------------------------------------+
|            Host Controller Driver (xHCI / EHCI)             |
+-------------------------------------------------------------+
                               |
                               v
                       [ Physical USB Bus ]
```

---

## 2. USB Descriptors Hierarchy

When a USB device is plugged in, the USB Core enumerates it by requesting a tree of standard descriptors:

```
[ Device Descriptor ]
         |
         v
[ Configuration Descriptor ]
         |
         v
[ Interface Descriptor ] (e.g. Mass Storage, Audio, Vendor)
         |
         +---> [ Endpoint Descriptor 1 ] (Bulk OUT)
         +---> [ Endpoint Descriptor 2 ] (Bulk IN)
```

1. **Device Descriptor (`struct usb_device_descriptor`)**: Contains VID, PID, device class, protocol revision, and number of configurations.
2. **Configuration Descriptor (`struct usb_config_descriptor`)**: Power requirements, bus/self-powered flags, and number of interfaces.
3. **Interface Descriptor (`struct usb_interface_descriptor`)**: Group of endpoints representing a discrete functional feature. Drivers bind at the **interface** level (`struct usb_interface`), not the device level!
4. **Endpoint Descriptor (`struct usb_endpoint_descriptor`)**: Address, direction (IN: device-to-host, OUT: host-to-device), transfer type (Control, Interrupt, Bulk, Isochronous), and maximum packet size (`wMaxPacketSize`).

---

## 3. The USB Request Block (URB)

All USB I/O in the Linux kernel is asynchronous and driven by **URBs (`struct urb`)**.

### URB Lifecycle:
1. **Creation**: Allocated with `usb_alloc_urb(iso_packets, GFP_KERNEL)`.
2. **Initialization**: Configured with target device pipe, buffer pointer, length, and completion callback:
   ```c
   usb_fill_bulk_urb(urb, udev,
                     usb_sndbulkpipe(udev, ep_out),
                     buffer, len,
                     complete_fn, context);
   ```
3. **Submission**: Handed off to the USB Core via `usb_submit_urb(urb, GFP_KERNEL)` (or `GFP_ATOMIC` if called in interrupt/atomic context).
4. **Hardware Execution**: The Host Controller schedules and executes bus transfers.
5. **Completion Notification**: When the transfer finishes (or fails), the USB core invokes `urb->complete(urb)` in softirq context.
6. **Disposal / Resubmission**: The callback inspects `urb->status`, processes data, and can either resubmit the URB or free it using `usb_free_urb(urb)`.

---

## 4. Driver Binding and Identification Table

The driver declares which devices it supports using `struct usb_device_id`:

```c
static const struct usb_device_id usb_table[] = {
    { USB_DEVICE(0x1234, 0x5678) },
    { } /* Null terminator */
};
MODULE_DEVICE_TABLE(usb, usb_table);
```

When a device with matching Vendor/Product ID is attached, USB Core calls the driver's `.probe` callback. When disconnected, `.disconnect` is called to cleanly unbind and release all active URBs.

---

## 5. Summary & Best Practices

- **Never block inside URB completion handlers**: The `complete` callback executes in softirq (interrupt) context. Do not sleep, allocate with `GFP_KERNEL`, or hold mutexes.
- **Always cancel active URBs upon disconnect**: Call `usb_kill_urb(urb)` in `.disconnect` before freeing buffers or driver structures.
- **Reference counting**: Use `usb_get_dev(udev)` and `usb_put_dev(udev)` to ensure the device structure is not freed while transfers are still pending.

