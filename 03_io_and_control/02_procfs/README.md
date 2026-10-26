# Chapter 09: Procfs Diagnostic Interface & Kernel seq_file Subsystem

Welcome to **Chapter 09** of the Linux Kernel Driver Journey! In this chapter, we explore `/proc` — the virtual in-memory filesystem used across UNIX and Linux systems to expose diagnostic metrics, internal device driver telemetry, and runtime tunables to system administrators and debugging tools.

---

## 1. Why Procfs?

While character device nodes (`/dev/`) represent hardware abstractions for direct data streaming or control operations, the `/proc` filesystem serves a distinct purpose:
- Exposing structured kernel telemetry (uptime, interrupt counters, cache statistics, active hardware profiles).
- Providing human-readable diagnostic views formatted as plain text (e.g. `cat /proc/cpuinfo`, `cat /proc/interrupts`).
- Allowing quick administrative tuning without recompiling or writing complex user-space C ioctl binaries (e.g. `echo "debug_level=2" > /proc/driver_diagnostics/config`).

---

## 2. Modern Kernel Architecture: `proc_ops` (Linux 5.6+)

Prior to Linux 5.6, procfs entries used the general VFS `struct file_operations`.
In Linux 5.6+, the kernel introduced **`struct proc_ops`** to decouple procfs from general VFS file operations, eliminating unused callbacks and optimizing memory footprint:

```c
struct proc_ops {
    unsigned int proc_flags;
    int (*proc_open)(struct inode *, struct file *);
    ssize_t (*proc_read)(struct file *, char __user *, size_t, loff_t *);
    ssize_t (*proc_read_iter)(struct kiocb *, struct iov_iter *);
    ssize_t (*proc_write)(struct file *, const char __user *, size_t, loff_t *);
    loff_t (*proc_lseek)(struct file *, loff_t, int);
    int (*proc_release)(struct inode *, struct file *);
    /* ... */
};
```

---

## 3. High-Performance Text Formatting: The `seq_file` Subsystem

Writing a naïve `read()` method for `/proc` that buffers text into a temporary string often breaks when the output exceeds a single page (`PAGE_SIZE`, 4096 bytes), leading to truncated output or memory leaks when user applications read in chunks (e.g. 128 bytes per `read()` call).

The kernel provides the **`seq_file`** API to handle:
- Arbitrary length, multi-page data generation.
- Automatic offset management, pagination, and buffer allocation.
- Safe printing with `seq_printf(m, format, ...)`, `seq_puts()`, and `seq_putc()`.

For single-page diagnostic outputs, the `single_open()` helper wraps `seq_file` into a single, clean show callback:

```c
static int status_proc_show(struct seq_file *m, void *v)
{
    seq_printf(m, "Driver Status : ACTIVE\n");
    seq_printf(m, "Uptime        : %llu s\n", uptime);
    return 0;
}

static int status_proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, status_proc_show, NULL);
}

static const struct proc_ops status_proc_ops = {
    .proc_open    = status_proc_open,
    .proc_read    = seq_read,
    .proc_lseek   = seq_lseek,
    .proc_release = single_release,
};
```

---

## 4. Lab Workflow: Build, Load, and Test

### Step 1: Compile the Module and Test Client
```bash
make
```

### Step 2: Load the Module
```bash
make load
```

### Step 3: Inspect Entries in `/proc`
You can inspect the entries directly with standard shell tools:
```bash
# View formatted diagnostic telemetry
cat /proc/driver_diagnostics/status

# Read runtime config
cat /proc/driver_diagnostics/config

# Dynamically modify config
echo "debug_level=2" | sudo tee /proc/driver_diagnostics/config
echo "profile=low_power_eco" | sudo tee /proc/driver_diagnostics/config
```

### Step 4: Run the User-Space Test Suite
The automated test client [`test_client.c`](file:///home/deepak/OPENSOURCE/LINUX_DRIVER/03_io_and_control/02_procfs/test_client.c) validates reading, updating, and error boundary rejection:
```bash
make test
```

Expected output:
```text
===================================================================
   Chapter 09: Procfs Diagnostic Subsystem Test Suite              
===================================================================

  [PASS] Test 1: Read /proc Status (seq_file)         (header & active status verified)

--- Diagnostic Snapshot ---
============================================
 Linux Driver Diagnostics (/proc Subsystem)
============================================
Driver Status        : ACTIVE
Kernel Module        : procfs_module
Uptime (seconds)     : 1
Diagnostic Reads     : 1
Config Writes        : 0
Active Debug Level   : 1
Hardware Profile     : default_high_performance
Simulated Core Temp  : 36.60 C
============================================
---------------------------

  [PASS] Test 2: Read /proc Config Entry              (keys debug_level & profile parsed)
  [PASS] Test 3: Update Config (debug_level=2)        (wrote debug_level=2 successfully)
  [PASS] Test 4: Update Config (profile=turbo_low_latency) (wrote new profile name)
  [PASS] Test 5: Verify Updated Configuration State   (persisted values matched)
  [PASS] Test 6: Verify Status Telemetry Synchronization (config writes counter = 2)
  [PASS] Test 7: Reject Malformed Syntax Rejection    (expected EINVAL=22, got errno=22: Invalid argument)
  [PASS] Test 8: Reject Out-Of-Bounds Parameter       (expected EINVAL=22, got errno=22: Invalid argument)

===================================================================
   Test Results: 8 Passed, 0 Failed (Total: 8)
===================================================================
```

### Step 5: Unload the Module
```bash
make unload
```
All entries under `/proc/driver_diagnostics` are recursively unlinked using `proc_remove()`.
