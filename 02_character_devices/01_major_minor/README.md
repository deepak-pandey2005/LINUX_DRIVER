# Chapter 04: Major and Minor Device Number Allocation

Welcome to **Phase 2: Character Device Drivers**!

In Linux, almost everything hardware-related is presented to user space as a file inside `/dev` (e.g., `/dev/ttyS0`, `/dev/null`, `/dev/nvme0n1`). When user space interacts with a device file, how does the Linux Virtual Filesystem (VFS) know which device driver code should execute?

The answer lies in **Device Numbers** (`dev_t`): **Major** and **Minor** numbers.

---

## 1. Major vs. Minor Numbers

A Linux device number is a 32-bit integer (`dev_t`) split into two distinct parts:

```
+------------------------------------+------------------------------------+
|       12 bits: MAJOR NUMBER        |       20 bits: MINOR NUMBER        |
|      (Range: 0 to 4,095)           |      (Range: 0 to 1,048,575)       |
+------------------------------------+------------------------------------+
```

- **Major Number**: Identifies the **driver** associated with the device. When an application calls `open("/dev/mydevice")`, the kernel looks up the Major number in its internal device table (`chrdevs`) to determine which driver's file operations to invoke.
- **Minor Number**: Identifies the **specific device instance** or sub-function managed by that driver. For instance, a multi-port serial driver might have Major 4, with Minor 0 representing COM1, Minor 1 representing COM2, etc.

### Kernel Helper Macros (`<linux/kdev_t.h>`)
- `MAJOR(dev_t dev)`: Extracts the 12-bit major number.
- `MINOR(dev_t dev)`: Extracts the 20-bit minor number.
- `MKDEV(int major, int minor)`: Combines a major and minor number into a single `dev_t`.

---

## 2. Dynamic vs. Static Allocation

### Static Allocation (`register_chrdev_region`)
In older drivers, developers hardcoded a specific major number:
```c
int register_chrdev_region(dev_t from, unsigned count, const char *name);
```
**Problem**: If two drivers claim the same major number, the kernel rejects the second driver. Hardcoding leads to system configuration conflicts.

### Dynamic Allocation (`alloc_chrdev_region`) — Recommended!
Modern drivers ask the kernel to automatically pick an unused major number:
```c
int alloc_chrdev_region(dev_t *dev, unsigned baseminor, unsigned count, const char *name);
```
- `dev`: Pointer to `dev_t` where the assigned number will be stored.
- `baseminor`: First minor number requested (usually `0`).
- `count`: How many consecutive minor numbers to allocate.
- `name`: Driver identifier that will appear in `/proc/devices`.

### Releasing Numbers (`unregister_chrdev_region`)
On module unload, the numbers **must** be returned to the kernel pool:
```c
void unregister_chrdev_region(dev_t from, unsigned count);
```

---

## 3. Hands-on Lab Walkthrough

### Step 1: Compile the Module
```bash
make
```

### Step 2: Load the Module
```bash
make load
```
*(Runs `sudo insmod major_minor_module.ko`)*

Check `dmesg`:
```text
major_minor_module: ==================================================
major_minor_module: Initializing Character Device Number Lab...
major_minor_module: alloc_chrdev_region succeeded!
major_minor_module:   - Allocated dev_t (raw 32-bit): 0x0f800000
major_minor_module:   - Major Number: 248
major_minor_module:   - Base Minor Number: 0 (Range: 0 to 2)
major_minor_module:   - Registered Name in /proc/devices: 'custom_char_dev'
major_minor_module: Check: cat /proc/devices | grep custom_char_dev
major_minor_module: ==================================================
```

---

### Step 3: Inspect `/proc/devices`
Verify that the kernel dynamically recorded your driver in its master device directory:
```bash
make check_proc
```
*(Or run `cat /proc/devices | grep custom_char_dev`)*

You will see:
```text
248 custom_char_dev
```
*The kernel assigned Major Number `248` to our driver!*

---

### Step 4: Unload the Driver
```bash
make unload
```

Check `dmesg`:
```text
major_minor_module: Unregistering character device numbers (Major 248)...
major_minor_module: Device numbers released cleanly.
```

Verify `/proc/devices` again:
```bash
make check_proc
```
The entry `248 custom_char_dev` is gone—the number was safely released!

---

## 4. Next Step: Connecting Numbers to File Operations (`cdev`)
Registering a major and minor number reserves an ID for your driver, but the device cannot do I/O yet until we attach a **`struct cdev`** and implement **`file_operations`** (`open`, `read`, `write`, `release`). That is precisely what we will tackle in **Chapter 05**!
