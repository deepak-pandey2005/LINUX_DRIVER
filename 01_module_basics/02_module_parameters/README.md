# Chapter 02: Kernel Module Parameters & Sysfs Interface

In production drivers, hardcoding hardware configurations (such as I2C bus speeds, GPIO pin assignments, debug log verbosity, or DMA buffer sizes) makes your driver inflexible. 

The Linux kernel provides the **Module Parameter API** (`<linux/moduleparam.h>`), allowing drivers to:
1. Accept configuration values at load time (`insmod mydriver.ko param=val`).
2. Expose variables directly to the **Sysfs Virtual Filesystem** (`/sys/module/<driver>/parameters/`).
3. Allow system administrators to tune driver behavior at runtime via simple file writes.
4. Execute custom validation callbacks whenever an administrator modifies a setting.

---

## 1. Code Breakdown: `param_module.c`

### A. Parameter Definition & Description
```c
static int sample_rate = 1000;
module_param(sample_rate, int, 0644);
MODULE_PARM_DESC(sample_rate, "Sampling rate in Hz (default: 1000, sysfs: RW)");
```
- `sample_rate`: The C variable to populate.
- `int`: The parameter data type. Supported types: `byte`, `short`, `ushort`, `int`, `uint`, `long`, `ulong`, `charp` (C string), `bool`, `invbool`.
- `0644`: Sysfs file permission mode bits:
  - `0444` (`S_IRUGO`): Read-only by everyone.
  - `0644` (`S_IRUGO | S_IWUSR`): Readable by everyone, writable only by `root`.
  - `0`: Parameter is only readable at load time; not exposed in `/sys/`.
  > **Security Rule**: The kernel compiler will reject world-writable permissions (`0666` or `S_IWOTH`) with a build error for security safety!

### B. Passing Comma-Separated Arrays
```c
static int threshold_levels[MAX_ARRAY_ELEMS] = { 10, 20, 30, 40, 50 };
static int threshold_count = MAX_ARRAY_ELEMS;

module_param_array(threshold_levels, int, &threshold_count, 0644);
```
- Accepts comma-separated values (e.g. `threshold_levels=5,15,25`).
- `&threshold_count`: The kernel writes the actual number of values supplied by the user into this variable.

### C. Dynamic Parameter Notification & Validation Callbacks
When an administrator modifies a parameter in `/sys/`, the driver often needs to validate the input or update hardware registers immediately.

We accomplish this using `struct kernel_param_ops`:
```c
static int notify_param_set_speed(const char *val, const struct kernel_param *kp)
{
    int temp_val, ret;
    ret = kstrtoint(val, 10, &temp_val);
    if (ret != 0 || temp_val < 1 || temp_val > 200) {
        pr_err("Rejected speed_limit %d! Must be between 1 and 200.\n", temp_val);
        return -EINVAL; // Rejects the write with Invalid Argument error
    }
    speed_limit = temp_val;
    return 0;
}

static const struct kernel_param_ops speed_param_ops = {
    .set = notify_param_set_speed,
    .get = notify_param_get_speed,
};

module_param_cb(speed_limit, &speed_param_ops, &speed_limit, 0644);
```

---

## 2. Hands-on Lab Walkthrough

### Step 1: Compile the Module
```bash
make
```

### Step 2: Query Parameter Help via `modinfo`
Before loading the driver, check what parameters it accepts and their descriptions:
```bash
make status
```
Or directly:
```bash
modinfo -p param_module.ko
```

Output:
```text
sample_rate:Sampling rate in Hz (default: 1000, sysfs: RW) (int)
device_tag:Alphanumeric device tag identifier (sysfs: RO) (charp)
verbose_logging:Enable verbose initialization output (0=no, 1=yes) (bool)
threshold_levels:Comma-separated threshold array up to 5 elements (sysfs: RW) (array of int)
speed_limit:Speed limit with range validation [1-200] (sysfs: RW)
```

---

### Step 3: Test A — Loading with Custom Arguments
Load the module while passing custom values on the command line:
```bash
make load_custom
```
*(Under the hood, this executes:)*
```bash
sudo insmod param_module.ko sample_rate=5000 device_tag="SENSOR_ALPHA" verbose_logging=1 speed_limit=120 threshold_levels=5,15,25,35
```

Check `dmesg` output:
```bash
sudo dmesg | tail -n 15
```
You will see:
```text
param_module: ==================================================
param_module: param_module loaded successfully!
param_module: Parameters received at load-time:
param_module:   - sample_rate:      5000 Hz
param_module:   - device_tag:       SENSOR_ALPHA
param_module:   - verbose_logging:  ENABLED
param_module:   - speed_limit:      120
param_module:   - threshold_levels: [4 elements]: 5, 15, 25, 35
param_module: Check /sys/module/param_module/parameters/ to view & edit!
param_module: ==================================================
```

---

### Step 4: Test B — Runtime Sysfs Inspection & Modification
Notice how sysfs exposes each parameter:
```bash
ls -l /sys/module/param_module/parameters/
```
Output:
```text
-r--r--r-- 1 root root 4096 device_tag       (0444: Read-Only)
-rw-r--r-- 1 root root 4096 sample_rate      (0644: Read-Write)
-rw-r--r-- 1 root root 4096 speed_limit      (0644: Read-Write)
-rw-r--r-- 1 root root 4096 threshold_levels (0644: Read-Write)
```
*(Notice `verbose_logging` is absent because permissions were set to `0`)*

#### Read current values:
```bash
cat /sys/module/param_module/parameters/sample_rate
cat /sys/module/param_module/parameters/speed_limit
```

#### Dynamically modify `speed_limit` at runtime:
```bash
echo 180 | sudo tee /sys/module/param_module/parameters/speed_limit
```

Check `dmesg`:
```text
param_module: >>> speed_limit updated dynamically: 120 -> 180 <<<
```

#### Attempt an invalid write (out-of-range value):
```bash
echo 999 | sudo tee /sys/module/param_module/parameters/speed_limit
```
Result:
```text
tee: /sys/module/param_module/parameters/speed_limit: Invalid argument
```
The driver's custom `.set` callback safely blocked the write and logged:
```text
param_module: Rejected speed_limit 999! Must be between 1 and 200.
```

*(You can run all of the above tests automatically with `make test_sysfs`)*

---

### Step 5: Unload the Driver
```bash
make unload
```

Check `dmesg`:
```text
param_module: param_module unloading.
param_module: Final speed_limit was: 180
param_module: Goodbye!
```
