# Chapter 05: `struct cdev` & File Operations (`open`, `release`, `read`, `write`)

In Chapter 04, we allocated a device number (`dev_t`). Now we connect that number to the **Linux Virtual Filesystem (VFS)** so user-space programs can perform real read/write operations on our device.

This lab implements a **virtual character device with an internal kernel memory buffer**, complete with a user-space C test application that verifies round-trip read and write integrity.

---

## 1. Key Kernel Concepts

### A. The Character Device Object (`struct cdev`)
Declared in `<linux/cdev.h>`, `struct cdev` is the kernel's internal representation of a character device:
```c
struct cdev my_cdev;

cdev_init(&my_cdev, &fops);  // Connects struct cdev with file_operations
ret = cdev_add(&my_cdev, dev_num, count); // Registers device with VFS
```

> [!IMPORTANT]
> **Order of Initialization (The `cdev_add` Invariant)**:
> Once `cdev_add()` returns successfully, the device is **immediately LIVE** and user space can begin invoking `open()`, `read()`, and `write()`. Therefore, all internal buffers, locks, and driver state **must be fully initialized BEFORE calling `cdev_add()`**!

---

### B. The File Operations Table (`struct file_operations`)
Declared in `<linux/fs.h>`, this structure contains pointers to functions that handle VFS system calls:
```c
static const struct file_operations fops = {
    .owner   = THIS_MODULE,
    .open    = dev_open,
    .release = dev_release,
    .read    = dev_read,
    .write   = dev_write,
};
```

---

### C. The `open()` Callback & `file->private_data`
```c
static int dev_open(struct inode *inode, struct file *file)
{
    struct custom_device_data *data = container_of(inode->i_cdev, struct custom_device_data, cdev);
    file->private_data = data;
    return 0;
}
```
- **`struct inode`**: Represents the physical/virtual file entity on the OS. `inode->i_cdev` points to the `struct cdev` that was registered.
- **`container_of()` macro**: Computes the base address of our containing structure `struct custom_device_data` from its member pointer `&cdev`.
- **`file->private_data`**: Stores per-open driver context so subsequent `read()` and `write()` calls do not need global lookups.

---

### D. Safe Memory Transfer: `copy_to_user` and `copy_from_user`
You must **never** dereference a user-space pointer directly in kernel space (e.g., `*user_buf = data`).
User pointers can be unmapped, paged out to swap, or maliciously crafted to exploit Ring 0.

Always use the safe memory transfer helpers declared in `<linux/uaccess.h>`:
```c
// Copying from kernel buffer to user space
unsigned long not_copied = copy_to_user(user_buf, kernel_buf, bytes);

// Copying from user space into kernel buffer
unsigned long not_copied = copy_from_user(kernel_buf, user_buf, bytes);
```
Both functions return the number of bytes that could **NOT** be copied. If `not_copied != 0`, return `-EFAULT`.

---

## 2. Hands-on Lab Walkthrough

### Step 1: Compile Module and Test Application
```bash
make
```
*(Compiles both `cdev_module.ko` and user-space `test_app`)*

---

### Step 2: Load the Driver
```bash
make load
```
*(Runs `sudo insmod cdev_module.ko`)*

Check `dmesg` to find the dynamically assigned Major number:
```text
cdev_module: Allocated Major: 248, Minor: 0
cdev_module: cdev_add registered successfully!
```

---

### Step 3: Create the Device Node in `/dev`
Before user-space programs can open the device, a special device node file must exist in `/dev`:
```bash
make create_node
```
*(This automatically discovers the Major number from `/proc/devices` and runs:)*
```bash
sudo mknod /dev/cdev_buffer_dev c <MAJOR> 0
sudo chmod 666 /dev/cdev_buffer_dev
```

Inspect the created device node:
```bash
ls -l /dev/cdev_buffer_dev
```
Output:
```text
crw-rw-rw- 1 root root 248, 0 Oct 22 20:15 /dev/cdev_buffer_dev
```
*(Notice the `c` indicating character device, and major 248, minor 0)*

---

### Step 4: Run the User-Space Test Application
```bash
make test
```
*(Or directly `./test_app`)*

Output:
```text
====================================================
User-Space Test Client: Character Device I/O
====================================================
[1] Opening device: /dev/cdev_buffer_dev...
[+] Successfully opened device (fd = 3)

[2] Writing to kernel driver: "Linux Driver Development: Round-trip User-to-Kernel buffer test!" (64 bytes)
[+] Successfully wrote 64 bytes to driver!

[3] Reading back from kernel driver...
[+] Successfully read 64 bytes!
    Data received from kernel: "Linux Driver Development: Round-trip User-to-Kernel buffer test!"

[SUCCESS] Read/Write test PASSED! Kernel buffer matched user data.
[4] Device closed.
====================================================
```

---

### Step 5: Test Shell I/O with Standard CLI Tools
Because Linux devices behave like regular files, you can also test using standard shell commands:

```bash
# Write into the driver
echo "Antigravity Linux Driver Lab" > /dev/cdev_buffer_dev

# Read back from the driver
cat /dev/cdev_buffer_dev
```
Output:
```text
Antigravity Linux Driver Lab
```

---

### Step 6: View Kernel Logs
```bash
make logs
```
```text
cdev_module: Device opened! Active open handles: 1 (PID: 12345, Comm: test_app)
cdev_module: Wrote 64 bytes into kernel buffer (total stored: 64)
cdev_module: Device closed! Remaining handles: 0
cdev_module: Device opened! Active open handles: 1 (PID: 12345, Comm: test_app)
cdev_module: Read 64 bytes (new offset: 64)
cdev_module: Device closed! Remaining handles: 0
```

---

### Step 7: Clean Up and Unload
```bash
make unload
```
*(Automatically removes `/dev/cdev_buffer_dev` and runs `sudo rmmod cdev_module`)*
