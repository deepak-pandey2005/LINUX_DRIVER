# Chapter 23: Kernel Debugging, Tracing & Kprobes

## Overview
This lab demonstrates runtime dynamic tracing and inspection of Linux kernel routines using **Kprobes (`struct kprobe`)**. You will learn how the kernel dynamically splices breakpoints into running routines without recompilation, hooks execution via `pre_handler` and `post_handler` callbacks, extracts CPU registers (`struct pt_regs`), and monitors system behavior with minimal overhead.

## Key Concepts
- **`struct kprobe`**: The core data structure defining target symbol name or virtual memory address.
- **`register_kprobe()` / `unregister_kprobe()`**: Kernel APIs for attaching and detaching dynamic probes.
- **`pre_handler`**: Callback invoked immediately before the target instruction executes; inspects register states (`pt_regs`), parameters, and caller PID.
- **`post_handler`**: Callback invoked immediately after single-stepping the target instruction.
- **Dynamic Debugging (`dyndbg`)**: Controlling runtime `pr_debug()` statements without recompilation via `/sys/kernel/debug/dynamic_debug/control`.
- **Telemetry via `/proc/kprobe_stats`**: Atomic counters and last intercepted caller information exposed to user-space.

## Building and Running

### 1. Build Module and Client
```bash
make
```

### 2. Load Module (Default Target: `do_sys_openat2`)
```bash
sudo insmod debug_kprobe_module.ko
dmesg | tail -n 10
```

*(Optional) Target a different kernel symbol via module parameter:*
```bash
sudo insmod debug_kprobe_module.ko symbol_name=kernel_clone
```

### 3. Verify Telemetry Interface
```bash
cat /proc/kprobe_stats
```

### 4. Run the Verification Client
```bash
./test_client
```
The client triggers file operations, and you will see the hit counters increment and the last caller PID/comm update to match the test client!

### 5. Clean Up
```bash
sudo rmmod debug_kprobe_module
make clean
```

