# Chapter 06: Device Classes, Sysfs & Dynamic Node Creation with Udev

In early Linux systems, `/dev` contained thousands of pre-created, static special device files for every conceivable piece of hardware. This wasted disk inodes, made `/dev` bloated, and failed whenever hot-pluggable hardware (like USB or hot-plug PCI) was inserted.

Today, Linux uses a clean, event-driven architecture combining **Sysfs Device Classes** and the user-space **udev** daemon. In this chapter, we explore how `class_create()` and `device_create()` emit kernel events to user space to dynamically populate `/dev`.

---

## 1. Evolution of the `/dev` Filesystem

```
+--------------------------------------------------------------------------+
|  Linux 2.0 - 2.4: Static /dev                                            |
|  - Thousands of static nodes created by MAKEDEV script.                  |
|  - No support for dynamic major numbers. Wasted disk inodes.             |
+--------------------------------------------------------------------------+
                                     |
                                     v
+--------------------------------------------------------------------------+
|  Linux 2.4 - 2.6: Devfs (In-Kernel Devfs)                                |
|  - The kernel itself mounted a filesystem at /dev and created nodes.     |
|  - Abandoned because of severe locking deadlocks and kernel bloat.       |
+--------------------------------------------------------------------------+
                                     |
                                     v
+--------------------------------------------------------------------------+
|  Linux 2.6 to Present: Sysfs + Udev (The Gold Standard)                  |
|  - Kernel exports hardware hierarchy to sysfs (/sys).                    |
|  - Kernel sends Netlink "uevents" when devices appear or disappear.      |
|  - User-space daemon (udevd / systemd-udevd) listens and creates nodes.  |
+--------------------------------------------------------------------------+
```

---

## 2. The Netlink Uevent Mechanism

When a driver calls:
```c
device_create(cls, NULL, dev_num, NULL, "auto_char_dev");
```

Behind the scenes, the kernel constructs a **Kobject Uevent** message and broadcasts it across a specialized Netlink socket (`NETLINK_KOBJECT_UEVENT`):

```text
ACTION=add
DEVPATH=/class/auto_char_class/auto_char_dev
SUBSYSTEM=auto_char_class
MAJOR=248
MINOR=0
DEVNAME=auto_char_dev
SEQNUM=3492
```

The user-space daemon **`systemd-udevd`** receives this Netlink packet:
1. It reads the `MAJOR` and `MINOR` numbers.
2. It evaluates any administrator rules matching `SUBSYSTEM=="auto_char_class"`.
3. It creates the node `/dev/auto_char_dev` using the standard `mknod()` system call.
4. It sets the file ownership and permissions (using the driver's `devnode` callback or udev rules).

When `device_destroy()` is called on module unload, the kernel emits an `ACTION=remove` uevent, causing `udevd` to automatically delete the file from `/dev`.

---

## 3. Kernel Device Classes (`struct class`)

A device class represents a high-level functional category of devices, regardless of the underlying bus. 
Examples of standard kernel classes:
- `/sys/class/net/` - Network interfaces (Ethernet, Wi-Fi).
- `/sys/class/tty/` - Serial ports, virtual terminals.
- `/sys/class/block/` - Storage devices.
- `/sys/class/gpio/` - GPIO controllers.

### Modern `class_create` API (Kernel >= 6.4 and 7.x+)
In older kernels (prior to 6.4), the signature was:
```c
// LEGACY (Linux < 6.4):
struct class *class_create(struct module *owner, const char *name);
```
In modern kernels (6.4+ through 7.x+):
```c
// MODERN (Linux >= 6.4+):
struct class *class_create(const char *name);
```
The `owner` parameter was removed because class structures are now tracked automatically by the driver core.

---

## 4. The Linux Error Handling Paradigm: `ERR_PTR` and `goto`

Kernel APIs frequently return pointers (like `struct class *` or `struct device *`). How does a kernel function return an error code (such as `-ENOMEM` or `-EINVAL`) if the return type is a pointer?

### The Negative Error Pointer Hack
The Linux kernel memory map reserves the topmost page of virtual memory (`0xfffffffffffff000` to `0xffffffffffffffff` on 64-bit systems) so it will never contain a valid kernel object pointer.

The kernel encodes negative error numbers directly into pointer values:
```c
#define MAX_ERRNO 4095
#define IS_ERR_VALUE(x) unlikely((unsigned long)(void *)(x) >= (unsigned long)-MAX_ERRNO)

static inline void *ERR_PTR(long error) {
    return (void *) error;
}

static inline long PTR_ERR(const void *ptr) {
    return (long) ptr;
}

static inline bool IS_ERR(const void *ptr) {
    return IS_ERR_VALUE((unsigned long)ptr);
}
```

Whenever an API returns a pointer:
1. Check validity with `IS_ERR(ptr)`.
2. Extract the integer error code with `PTR_ERR(ptr)`.

### The Canonical `goto` Unwinding Pattern
In the Linux Kernel Style Guide, `goto` is not considered harmful; it is the **mandatory standard** for resource unwinding:

```c
static int __init my_driver_init(void)
{
    int ret;

    ret = step1_allocate_numbers();
    if (ret)
        return ret;

    ret = step2_init_cdev();
    if (ret)
        goto err_step1;

    ret = step3_create_class();
    if (ret)
        goto err_step2;

    ret = step4_create_device();
    if (ret)
        goto err_step3;

    return 0; // All steps succeeded!

err_step3:
    class_destroy(cls);
err_step2:
    cdev_del(&cdev);
err_step1:
    unregister_chrdev_region(dev, 1);
    return ret;
}
```

This guarantees that:
- Every partially allocated resource is freed in reverse order of its allocation.
- No memory leaks occur on initialization failures.
- No duplicate cleanup code is written.
