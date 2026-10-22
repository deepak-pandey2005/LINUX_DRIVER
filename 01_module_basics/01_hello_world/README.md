# Chapter 01: Hello World Kernel Module

Welcome to your first step into Linux Kernel Driver Development! This lab introduces the minimal anatomy of a **Loadable Kernel Module (LKM)**, how to compile it against your running kernel headers, insert it into Ring 0 (kernel space), view kernel ring buffer logs, and safely unload it.

---

## 1. What is a Loadable Kernel Module (LKM)?

In traditional operating system design:
- **Monolithic Kernels** (like Linux) run all operating system services, drivers, and scheduler in a single shared address space in privileged CPU mode (Ring 0).
- If every driver had to be compiled directly into the kernel binary (`vmlinux`), you would need to recompile and reboot the kernel every time you added a device or updated a driver.

**LKMs solve this problem**:
- They are object files (`.ko` - Kernel Object) that can be dynamically linked into the running kernel at runtime without rebooting.
- When no longer needed, they can be unlinked and unloaded, reclaiming RAM.

---

## 2. Code Breakdown: `hello_module.c`

### A. The Module Prefix Macro
```c
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt
```
- In modern kernel code, we avoid hardcoding prefixes in print statements.
- By defining `pr_fmt`, every call to `pr_info()`, `pr_err()`, etc., automatically prepends the module's name (`hello_module: `) to the message in `dmesg`.

### B. Header Inclusions
- `<linux/init.h>`: Declares initialization and cleanup macros like `__init`, `__exit`, `module_init`, and `module_exit`.
- `<linux/module.h>`: Core definitions for LKMs (`MODULE_LICENSE`, `MODULE_AUTHOR`, symbol export structures).
- `<linux/kernel.h>`: Kernel-level utilities including print macros and system constants (`UTS_RELEASE`).

> **Note**: You CANNOT include standard C library headers (like `<stdio.h>`, `<stdlib.h>`) in kernel modules. User-space C library runtime (libc) does not exist in kernel space.

### C. Initialization Function
```c
static int __init hello_init(void)
```
- **`__init` attribute**: Informs the compiler (`gcc`) to place this function code in a special ELF section called `.init.text`. Once the kernel completes running this init function during `insmod`, the kernel frees this entire memory section back to the OS!
- **Return value**: Returning `0` signals success. If you return a negative error number (e.g., `-ENOMEM`, `-EFAULT`), the kernel aborts loading and automatically cleans up.

### D. Cleanup Function
```c
static void __exit hello_exit(void)
```
- **`__exit` attribute**: Informs the compiler to place this function in the `.exit.text` section. If this module is ever built built-in (`y` in Kconfig instead of `m`), the exit function is discarded entirely because a built-in driver cannot be unloaded.

### E. Registration Macros
```c
module_init(hello_init);
module_exit(hello_exit);
```
- These macros generate function pointers placed in special kernel ELF tables so the kernel loader knows which function to execute on load and unload.

### F. Module Licensing & Metadata
```c
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Deepak Pandey");
MODULE_DESCRIPTION("A robust introductory Linux Loadable Kernel Module (LKM)");
MODULE_VERSION("1.0.0");
```
- **`MODULE_LICENSE("GPL")`**: Highly important! If you do not specify a GPL-compatible license, the kernel marks itself as **"tainted"** and disables access to GPL-only kernel symbols (`EXPORT_SYMBOL_GPL`).

---

## 3. How to Build & Run

### Step 1: Compile the Module
Run `make` in this directory:
```bash
make
```
This invokes the kernel build system (Kbuild). The output binary will be `hello_module.ko`.

### Step 2: Inspect Module Metadata
Use `modinfo` to view the compiled module's attributes without loading it:
```bash
modinfo hello_module.ko
```

### Step 3: Load the Module into the Kernel
Use `insmod` (requires root privileges):
```bash
sudo insmod hello_module.ko
```
*(Or simply run `make load`)*

### Step 4: Verify Module is Loaded
Check the loaded module list via `/proc/modules`:
```bash
lsmod | grep hello_module
```

### Step 5: Check Kernel Logs
View the kernel ring buffer using `dmesg`:
```bash
sudo dmesg | tail -n 15
```
*(Or run `make logs`)*

You will see:
```text
hello_module: ========================================
hello_module: Hello from the Linux Kernel!
hello_module: Module loaded successfully into kernel space.
hello_module: Running under kernel version: 7.2.10-arch1-1
hello_module: ========================================
```

### Step 6: Unload the Module
Unload the module using `rmmod`:
```bash
sudo rmmod hello_module
```
*(Or run `make unload`)*

Check `dmesg` again:
```bash
hello_module: Goodbye from the Linux Kernel!
hello_module: Module unloaded cleanly. Memory reclaimed.
```

### Step 7: Clean Up Build Files
```bash
make clean
```

---

## 4. Key Takeaways & Common Traps
1. **Never use floating point math** (`float`, `double`) in kernel code; the FPU state is not preserved across kernel context switches.
2. **Kernel stack is tiny**: Typically only 8KB to 16KB per thread. Never allocate large arrays or buffers on the stack (e.g., `char buf[10240]` will trigger a kernel stack overflow panic). Use dynamic allocation (`kmalloc`) instead.
3. **No libc**: You have kernel replacements (`printk`, `pr_info`, `snprintf`, `memcpy`, `memset`, `kmalloc`, `kfree`).
