# Chapter 26: Linux Input Subsystem & Virtual Controller Driver

## 1. Architectural Overview of the Input Subsystem

The **Linux Input Subsystem** provides a centralized, hardware-agnostic framework for managing all user input hardware—including keyboards, mice, trackpads, joysticks, game controllers, and multi-touch touchscreens.

The subsystem is composed of three distinct functional layers:

```
+--------------------------------------------------------------------+
|                    User Space Applications                         |
|             (X11 / Wayland, SDL2, evtest, Game Engines)            |
+--------------------------------------------------------------------+
                                  |
                                  v
+--------------------------------------------------------------------+
|                       Input Event Handlers                         |
|   evdev (/dev/input/eventX) | joydev (/dev/input/jsX) | mousedev   |
+--------------------------------------------------------------------+
                                  |
                                  v
+--------------------------------------------------------------------+
|                      Linux Input Core Subsystem                    |
|      - Event Filtering, Timestamping & Multiplexing                |
|      - struct input_dev Lifecycle Management                       |
+--------------------------------------------------------------------+
                                  |
                                  v
+--------------------------------------------------------------------+
|                     Input Device Drivers                           |
|       (USB HID, I2C Touchscreens, vcontroller_driver)              |
+--------------------------------------------------------------------+
```

---

## 2. Event Types & Bitmap Capabilities

Input devices must declare which event types and codes they are capable of generating before registering with the core:

| Event Type | Macro | Bitmask Field | Typical Input Codes |
|---|---|---|---|
| **Synchronization** | `EV_SYN` | Implicit | `SYN_REPORT`, `SYN_CONFIG`, `SYN_MT_REPORT` |
| **Keys / Buttons** | `EV_KEY` | `dev->keybit` | `KEY_A`, `KEY_ENTER`, `BTN_SOUTH` (A), `BTN_EAST` (B) |
| **Relative Motion**| `EV_REL` | `dev->relbit` | `REL_X`, `REL_Y`, `REL_WHEEL` |
| **Absolute Motion**| `EV_ABS` | `dev->absbit` | `ABS_X`, `ABS_Y`, `ABS_PRESSURE`, `ABS_MT_POSITION_X` |
| **Miscellaneous**  | `EV_MSC` | `dev->mscbit` | `MSC_SCAN`, `MSC_TIMESTAMP` |

### Setting Capabilities:
```c
/* Enable button capabilities */
set_bit(EV_KEY, dev->evbit);
set_bit(BTN_A, dev->keybit);
set_bit(BTN_B, dev->keybit);

/* Enable absolute analog axes with bounds */
set_bit(EV_ABS, dev->evbit);
input_set_abs_params(dev, ABS_X, -512, 512, 0, 0); // (dev, axis, min, max, fuzz, flat)
```

---

## 3. Atomic Event Batching (`input_sync`)

Physical events often require multiple parameters (e.g. an analog joystick displacement modifies both X and Y axes simultaneously).

To avoid exposing inconsistent intermediate states to user-space, the Linux input core buffers reported changes until `input_sync()` is called:

```c
/* Update X and Y simultaneously */
input_report_abs(dev, ABS_X, 250);
input_report_abs(dev, ABS_Y, -100);

/* Commit atomic packet to user space with EV_SYN */
input_sync(dev);
```

When `input_sync(dev)` is called, the core emits a `SYN_REPORT` event. User-space event loops reading `/dev/input/eventX` read until they see `type == EV_SYN && code == SYN_REPORT` before updating the application state.

---

## 4. User Space Event Data Structure (`struct input_event`)

Applications reading `/dev/input/eventX` receive a stream of `struct input_event` objects:

```c
struct input_event {
    struct timeval time; // Event timestamp
    __u16 type;          // EV_KEY, EV_ABS, EV_REL, EV_SYN
    __u16 code;          // BTN_A, ABS_X, etc.
    __s32 value;         // 1 (down), 0 (up), or axis coordinate
};
```

---

## 5. Summary & Best Practices

- Always call `input_sync(dev)` after an event sequence to flush the event queue.
- Use `clamp()` or hardware calibration checks before emitting `input_report_abs()` to ensure coordinates stay within declared `[min, max]` bounds.
- **Never call `input_free_device()` after `input_unregister_device()`**: `input_unregister_device()` automatically drops the internal reference count.
