# Chapter 03: Multi-File Modules & Exporting Symbols (`EXPORT_SYMBOL_GPL`)

Real-world device drivers are rarely written in a single monolithic C source file. They are architected into modular subsystems (e.g., protocol handlers, hardware register abstractions, and mathematical routines).

Furthermore, in complex hardware architectures, one driver often provides core services that other drivers depend on (for example, a power-management or bus controller driver providing helper APIs to peripheral sensor drivers).

This chapter is split into two practical parts:
1. **Part 1 (`part1_multifile`)**: Compiling multiple distinct C source files into a single `.ko` kernel object using Kbuild.
2. **Part 2 (`part2_shared_symbols`)**: Exporting functions and variables into the global kernel symbol table (`EXPORT_SYMBOL_GPL`) and managing inter-module dependencies.

---

## 🏗️ Part 1: Multi-File Module Compilation

Located in: [`part1_multifile/`](file:///home/deepak/OPENSOURCE/LINUX_DRIVER/01_module_basics/03_export_symbols/part1_multifile/)

### The Kbuild Multi-File Pattern
To combine multiple `.c` files into a single module named `multi_file_driver.ko`, the Makefile defines:
```makefile
obj-m := multi_file_driver.o
multi_file_driver-objs := multi_main.o sensor_math.o
```

> **Golden Rule of Kbuild**: Never name any source file identically to the final module name! If the module is `multi_file_driver.ko`, you must NOT have a `multi_file_driver.c`. Kbuild will get confused and produce circular dependency build errors. Name your entry point `multi_main.c` or `main.c`.

### Files:
- [sensor_math.h](file:///home/deepak/OPENSOURCE/LINUX_DRIVER/01_module_basics/03_export_symbols/part1_multifile/sensor_math.h): Prototypes for sensor math functions.
- [sensor_math.c](file:///home/deepak/OPENSOURCE/LINUX_DRIVER/01_module_basics/03_export_symbols/part1_multifile/sensor_math.c): Helper algorithms (no `module_init`/`module_exit`).
- [multi_main.c](file:///home/deepak/OPENSOURCE/LINUX_DRIVER/01_module_basics/03_export_symbols/part1_multifile/multi_main.c): Main driver entry point calling `sensor_math` routines.

### Try Part 1:
```bash
cd part1_multifile
make
make load
make logs
make unload
```

---

## 🔗 Part 2: Shared Symbols Across Distinct Modules

Located in: [`part2_shared_symbols/`](file:///home/deepak/OPENSOURCE/LINUX_DRIVER/01_module_basics/03_export_symbols/part2_shared_symbols/)

### The Concept
When Driver B needs to call a function or read a variable defined in Driver A:
1. Driver A exports the symbol using `EXPORT_SYMBOL()` or `EXPORT_SYMBOL_GPL()`.
2. The kernel's symbol table (`/proc/kallsyms`) records the symbol and its memory address.
3. When Driver B is loaded, the kernel module loader automatically resolves the symbol address and increments Driver A's reference count.

### Code Highlights

#### Producer: [symbol_producer.c](file:///home/deepak/OPENSOURCE/LINUX_DRIVER/01_module_basics/03_export_symbols/part2_shared_symbols/symbol_producer.c)
```c
int global_driver_event_count = 100;
EXPORT_SYMBOL_GPL(global_driver_event_count);

u32 compute_device_checksum(const u8 *buffer, size_t len) { ... }
EXPORT_SYMBOL_GPL(compute_device_checksum);
```

#### Consumer: [symbol_consumer.c](file:///home/deepak/OPENSOURCE/LINUX_DRIVER/01_module_basics/03_export_symbols/part2_shared_symbols/symbol_consumer.c)
```c
#include "symbol_producer.h"

// Accesses and increments producer's exported variable directly!
global_driver_event_count += 5;

// Calls producer's exported function!
checksum = compute_device_checksum(payload, len);
```

---

## 🧪 Interactive Dependency Experiments

Run these tests in `part2_shared_symbols/`:

### Experiment 1: Dependency Failure (Consumer First)
What happens if you try to load the consumer before the producer is loaded?
```bash
make test_dependency_fail
```
**Output**:
```text
insmod: ERROR: could not insert module symbol_consumer.ko: Unknown symbol in module
```
*The kernel loader prevents invalid memory references before they happen!*

---

### Experiment 2: Correct Order Loading
Load the producer, then load the consumer:
```bash
make load_producer
make load_consumer
```

Inspect kernel logs:
```bash
make logs
```
You will see:
```text
symbol_producer: symbol_producer loaded successfully!
symbol_producer: Exported symbols into kernel table:
symbol_producer:   - compute_device_checksum() [EXPORT_SYMBOL_GPL]
symbol_producer:   - global_driver_event_count = 100 [EXPORT_SYMBOL_GPL]
...
symbol_consumer: symbol_consumer loaded successfully!
symbol_consumer: Invoking exported symbols from symbol_producer:
symbol_consumer:   - Current global_driver_event_count: 100
symbol_consumer:   - Incremented global_driver_event_count to: 105
symbol_consumer:   - Calculated payload checksum: 0x9f5100dc
```

---

### Experiment 3: Inspecting Dependency Graph & `/proc/kallsyms`
Run:
```bash
make check_dependency
```
Notice:
1. `lsmod` displays:
   ```text
   Module                  Size  Used by
   symbol_consumer        16384  0
   symbol_producer        16384  1 symbol_consumer
   ```
   *`symbol_producer` shows `Used by: 1 symbol_consumer`!*
2. `/proc/kallsyms` reveals the exported addresses:
   ```text
   ffffffffc08... r __ksymtab_compute_device_checksum [symbol_producer]
   ffffffffc08... T compute_device_checksum           [symbol_producer]
   ffffffffc08... D global_driver_event_count         [symbol_producer]
   ```

---

### Experiment 4: Protection Against Dangling Pointers
What happens if you try to unload the producer while consumer is still running?
```bash
make test_rmmod_fail
```
**Output**:
```text
rmmod: ERROR: Module symbol_producer is in use by: symbol_consumer
```
*The Linux kernel protects memory safety by refusing to let you pull the rug out from under the consumer!*

---

### Experiment 5: Clean Unload
Unload in the proper reverse order:
```bash
make unload_all
```
