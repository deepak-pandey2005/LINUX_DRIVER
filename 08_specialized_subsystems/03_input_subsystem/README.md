# Chapter 26: Linux Input Subsystem & Virtual Controller Driver

## Overview
This lab covers the architecture of the **Linux Input Subsystem**. You will learn how the kernel handles human interface devices (keyboards, mice, joysticks, gamepads, touchscreens), registers input devices using `struct input_dev`, advertises event capabilities (`EV_KEY`, `EV_REL`, `EV_ABS`), injects events into the input core (`input_report_*`), and batches atomic updates with `input_sync()`.

## Key Concepts
- **Linux Input Architecture**:
  - **Device Drivers**: Interface with physical/virtual hardware and emit events into the core.
  - **Input Core**: Dispatches events to registered event handlers.
  - **Event Handlers**: Standard handlers like `evdev` (`/dev/input/eventX`), `joydev` (`/dev/input/jsX`), and `mousedev`.
- **`struct input_dev`**:
  - Allocated with `input_allocate_device()`.
  - Registered with `input_register_device()`.
  - Unregistered with `input_unregister_device()`.
- **Event Types**:
  - `EV_KEY`: Binary states (keys, gamepad buttons).
  - `EV_REL`: Relative coordinate deltas (mouse wheels, trackballs).
  - `EV_ABS`: Absolute coordinate positions (analog sticks, touchscreens).
  - `EV_SYN`: Synchronization barrier (`SYN_REPORT`) marking the completion of an atomic input packet.

## Building and Running

### 1. Build Module and Client
```bash
make
```

### 2. Load the Module
```bash
sudo insmod input_driver_module.ko
dmesg | tail -n 10
```

### 3. Grant Permissions to Device Node
```bash
sudo chmod 666 /dev/vcontroller
```

### 4. Run the Verification Client
```bash
./test_client
```

### 5. Verify via Standard Linux Tooling
Inspect the registered input device:
```bash
cat /proc/bus/input/devices | grep -A 5 "Virtual Gamepad"
```
Or interactively trace live events using `evtest`:
```bash
sudo evtest
```

### 6. Clean Up
```bash
sudo rmmod input_driver_module
make clean
```
