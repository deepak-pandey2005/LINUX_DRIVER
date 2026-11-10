# 🐧 Linux Kernel Driver Development: Beginner to Advanced

A comprehensive, hands-on, production-grade guide and codebase for mastering **Linux Device Drivers, Kernel Modules, and Subsystems**.

Every chapter includes:
- **Production-grade C code** targeting modern Linux kernels (v6.x / v7.x+).
- **Standalone Kbuild Makefiles** (`make`, `make load`, `make unload`, `make logs`).
- **User-space testing applications** for hardware emulation and verification.
- **Deep-dive architectural documentation** explaining kernel internals, memory models, hardware abstractions, and synchronization.

---

## 🧭 Repository Structure

```text
LINUX_DRIVER/
├── docs/                                # In-depth theoretical and architectural guides
│   ├── README.md                        # Master curriculum index & roadmap
│   └── 01_kernel_module_architecture.md # Chapter 01: Kernel space vs user space, LKMs, Kbuild
├── 01_module_basics/                    # Phase 1: Fundamentals of Kernel Modules
│   └── 01_hello_world/                  # Lab 01: Hello World, lifecycle, pr_fmt, metadata
├── Makefile                             # Root build orchestrator
├── .gitignore                           # Kernel artifact exclusions (*.ko, *.mod, etc.)
└── README.md                            # This repository overview
```

---

## 🚀 Curriculum Overview

| Phase | Topics Covered | Status | Code & Lab | Architecture Doc |
| :--- | :--- | :---: | :--- | :--- |
| **Phase 1: Module Basics** | Hello World, Module Lifecycle, `pr_fmt`, `insmod`/`rmmod` | ✅ Ready | [01_hello_world](file:///home/deepak/OPENSOURCE/LINUX_DRIVER/01_module_basics/01_hello_world/README.md) | [Doc 01](file:///home/deepak/OPENSOURCE/LINUX_DRIVER/docs/01_kernel_module_architecture.md) |
| | Module Parameters (`module_param`, sysfs runtime configuration) | ✅ Ready | [02_module_parameters](file:///home/deepak/OPENSOURCE/LINUX_DRIVER/01_module_basics/02_module_parameters/README.md) | [Doc 02](file:///home/deepak/OPENSOURCE/LINUX_DRIVER/docs/02_module_parameters_and_sysfs.md) |
| | Multi-File Drivers & Exporting Symbols (`EXPORT_SYMBOL_GPL`) | ✅ Ready | [03_export_symbols](file:///home/deepak/OPENSOURCE/LINUX_DRIVER/01_module_basics/03_export_symbols/README.md) | [Doc 03](file:///home/deepak/OPENSOURCE/LINUX_DRIVER/docs/03_symbol_export_and_dependencies.md) |
| **Phase 2: Character Devices** | Major & Minor Numbers (`alloc_chrdev_region`, `dev_t`) | ✅ Ready | [04_major_minor](file:///home/deepak/OPENSOURCE/LINUX_DRIVER/02_character_devices/01_major_minor/README.md) | [Doc 04](file:///home/deepak/OPENSOURCE/LINUX_DRIVER/docs/04_character_device_numbers.md) |
| | `struct cdev` & File Operations (`open`, `release`, `read`, `write`) | ✅ Ready | [05_cdev_file_ops](file:///home/deepak/OPENSOURCE/LINUX_DRIVER/02_character_devices/02_cdev_file_ops/README.md) | [Doc 05](file:///home/deepak/OPENSOURCE/LINUX_DRIVER/docs/05_cdev_and_file_operations.md) |
| | Dynamic Device Nodes & udev (`class_create`, `device_create`) | ✅ Ready | [06_device_nodes](file:///home/deepak/OPENSOURCE/LINUX_DRIVER/02_character_devices/03_udev_device_creation/README.md) | [Doc 06](file:///home/deepak/OPENSOURCE/LINUX_DRIVER/docs/06_udev_and_sysfs_device_nodes.md) |
| | Safe Memory Transfer & Ring Buffer (`copy_to/from_user`) | ✅ Ready | [07_ring_buffer](file:///home/deepak/OPENSOURCE/LINUX_DRIVER/02_character_devices/04_ring_buffer/README.md) | [Doc 07](file:///home/deepak/OPENSOURCE/LINUX_DRIVER/docs/07_kernel_memory_access_and_circular_buffers.md) |
| **Phase 3: Control & Virtual FS** | IOCTL Interface (`_IO`, `_IOR`, `_IOW`, `_IOWR`) | ✅ Ready | [08_ioctl](file:///home/deepak/OPENSOURCE/LINUX_DRIVER/03_io_and_control/01_ioctl/README.md) | [Doc 08](file:///home/deepak/OPENSOURCE/LINUX_DRIVER/docs/08_ioctl_subsystem_and_hardware_control.md) |
| | Procfs Diagnostic Interface (`proc_create`, `proc_ops`) | ✅ Ready | [09_procfs](file:///home/deepak/OPENSOURCE/LINUX_DRIVER/03_io_and_control/02_procfs/README.md) | [Doc 09](file:///home/deepak/OPENSOURCE/LINUX_DRIVER/docs/09_procfs_diagnostic_interface.md) |
| | Sysfs & Kernel Objects (`kobject`, `attribute_group`) | ✅ Ready | [10_sysfs](file:///home/deepak/OPENSOURCE/LINUX_DRIVER/03_io_and_control/03_sysfs/README.md) | [Doc 10](file:///home/deepak/OPENSOURCE/LINUX_DRIVER/docs/10_sysfs_and_kobjects.md) |
| **Phase 4: Concurrency & Memory** | Spinlocks, Mutexes, Atomics (SMP safety & benchmarking) | ✅ Ready | [11_locking](file:///home/deepak/OPENSOURCE/LINUX_DRIVER/04_concurrency_and_memory/01_locking/README.md) | [Doc 11](file:///home/deepak/OPENSOURCE/LINUX_DRIVER/docs/11_concurrency_and_locking.md) |
| | Kernel Memory & Zero-Copy mmap (`remap_pfn_range`) | ✅ Ready | [12_memory_mmap](file:///home/deepak/OPENSOURCE/LINUX_DRIVER/04_concurrency_and_memory/02_memory_mmap/README.md) | [Doc 12](file:///home/deepak/OPENSOURCE/LINUX_DRIVER/docs/12_kernel_memory_management.md) |
| **Phase 5: Interrupts & Timers** | Hardware IRQs (`request_irq`, Top Half vs Bottom Half) | ✅ Ready | [13_interrupts](file:///home/deepak/OPENSOURCE/LINUX_DRIVER/05_interrupts_and_timers/01_interrupts/README.md) | [Doc 13](file:///home/deepak/OPENSOURCE/LINUX_DRIVER/docs/13_interrupt_handling.md) |
| | Workqueues & Threaded IRQs | ✅ Ready | [14_workqueues](file:///home/deepak/OPENSOURCE/LINUX_DRIVER/05_interrupts_and_timers/02_workqueues/README.md) | [Doc 14](file:///home/deepak/OPENSOURCE/LINUX_DRIVER/docs/14_deferred_execution.md) |
| | Kernel Timers (`timer_list`, `hrtimer`, `jiffies`, `msleep`) | ✅ Ready | [15_timers](file:///home/deepak/OPENSOURCE/LINUX_DRIVER/05_interrupts_and_timers/03_timers/README.md) | [Doc 15](file:///home/deepak/OPENSOURCE/LINUX_DRIVER/docs/15_timers_and_delays.md) |
| **Phase 6: Device Model & Buses** | Linux Device Model & Platform Drivers (`probe`, `remove`) | ✅ Ready | [16_platform_driver](file:///home/deepak/OPENSOURCE/LINUX_DRIVER/06_device_model_and_buses/01_platform_driver/README.md) | [Doc 16](file:///home/deepak/OPENSOURCE/LINUX_DRIVER/docs/16_linux_device_model.md) |
| | Device Tree (DTS/DTB, `of_device_id`, OF property parsing) | ✅ Ready | [17_device_tree](file:///home/deepak/OPENSOURCE/LINUX_DRIVER/06_device_model_and_buses/02_device_tree/README.md) | [Doc 17](file:///home/deepak/OPENSOURCE/LINUX_DRIVER/docs/17_device_tree.md) |
| | GPIO Subsystem (Modern `gpiod` Consumer API) | ✅ Ready | [18_gpio_driver](file:///home/deepak/OPENSOURCE/LINUX_DRIVER/06_device_model_and_buses/03_gpio_driver/README.md) | [Doc 18](file:///home/deepak/OPENSOURCE/LINUX_DRIVER/docs/18_gpio_subsystem.md) |
| | I2C Client Drivers (`i2c_driver`, SMBus, registers) | ✅ Ready | [19_i2c_sensor](file:///home/deepak/OPENSOURCE/LINUX_DRIVER/06_device_model_and_buses/04_i2c_sensor/README.md) | [Doc 19](file:///home/deepak/OPENSOURCE/LINUX_DRIVER/docs/19_i2c_bus_driver.md) |
| | DMA Engine (Coherent & Streaming DMA, Scatter-Gather) | 🔜 Next | `20_dma_engine` | `docs/20_dma_engine.md` |
| **Phase 7: Advanced Mastery** | Subsystems (Network `net_device`, USB, IIO) | ⏳ Queued | `21_subsystems` | `docs/21_subsystems.md` |
| | Tracing, Debugging, KASAN, Lockdep, Oops Analysis | ⏳ Queued | `22_debugging` | `docs/22_kernel_debugging.md` |

For the full detailed curriculum and architecture roadmap, see the [Curriculum Guide](file:///home/deepak/OPENSOURCE/LINUX_DRIVER/docs/README.md).

---

## ⚡ Quick Start: Chapter 01

To compile and test your first driver:

```bash
cd 01_module_basics/01_hello_world
make
```

Inspect module information:
```bash
modinfo hello_module.ko
```

Load the module into the running kernel:
```bash
sudo insmod hello_module.ko
```

View kernel log output:
```bash
sudo dmesg | tail -n 15
```

Unload the module:
```bash
sudo rmmod hello_module
```

Check logs again to verify clean unloading:
```bash
sudo dmesg | tail -n 15
```
