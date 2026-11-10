# Chapter 23: Kernel Debugging, Tracing & Kprobes

## 1. Overview of Kernel Debugging & Tracing Mechanisms

Debugging kernel space requires distinct techniques from user-space debugging: you cannot easily pause the entire system under a traditional debugger like GDB without a serial console or JTAG emulator (e.g., KGDB). Instead, the Linux kernel provides rich, non-disruptive, runtime observability subsystems:

| Tool / Mechanism | Level | Primary Use Case | Overhead |
|---|---|---|---|
| **Printk & Dynamic Debug (`dyndbg`)** | Source-level | Logging messages enabled/disabled per file or line at runtime | Minimal when disabled |
| **Kprobes / Kretprobes** | Instruction-level | Dynamic non-intrusive function breakpoints & return value inspection | Low (int3 breakpoint trap) |
| **Ftrace** | Subsystem / Function | Function call graphing, latency tracing, interrupt profiling | Very low |
| **eBPF (BPF Trace)** | Subsystem / Programmable | In-kernel sandboxed bytecode programs attached to tracepoints or kprobes | Very low |
| **KASAN & Lockdep** | Compile-time Sanitizers | Detecting out-of-bounds memory accesses, use-after-free, and lock inversions | High (development only) |

---

## 2. Kprobes Architecture & Execution Flow

Kprobes enables developers to attach callback handlers to almost any kernel code address at runtime.

### Mechanism:
1. **Registration**: When `register_kprobe(&kp)` is called, the kernel copies the target instruction and replaces the first byte with a breakpoint instruction (`int3` on x86, `BRK` on ARM64).
2. **Pre-Handler Trigger**: When CPU execution reaches the probe address, the breakpoint fault occurs. The Kprobe fault handler catches the exception and invokes your `pre_handler(struct kprobe *p, struct pt_regs *regs)`.
3. **Single Stepping**: The kernel single-steps the original saved instruction.
4. **Post-Handler Trigger**: Immediately after single-stepping finishes, the kernel invokes your `post_handler(struct kprobe *p, struct pt_regs *regs, unsigned long flags)`.
5. **Resume**: CPU execution continues along the normal kernel path.

```
       Normal Execution Path
                 |
                 v
   +-----------------------------+
   | Breakpoint Trap (int 3)     |
   +-----------------------------+
                 |
                 v
   +-----------------------------+
   | Execute kp->pre_handler()   |  <-- Inspect pt_regs, PID, args
   +-----------------------------+
                 |
                 v
   +-----------------------------+
   | Single-step original opcode |
   +-----------------------------+
                 |
                 v
   +-----------------------------+
   | Execute kp->post_handler()  |  <-- Telemetry & cleanup
   +-----------------------------+
                 |
                 v
     Resume Normal Execution
```

---

## 3. Dynamic Debugging (`dyndbg`)

Dynamic Debug allows `pr_debug()` and `dev_dbg()` statements to be dynamically turned on or off while the kernel is running without recompiling or rebooting.

To enable dynamic debug for our module:
```bash
echo "module debug_kprobe_module +p" | sudo tee /sys/kernel/debug/dynamic_debug/control
```
To disable:
```bash
echo "module debug_kprobe_module -p" | sudo tee /sys/kernel/debug/dynamic_debug/control
```

---

## 4. Kernel Oops & Panic Analysis

When a fatal bug occurs (NULL pointer dereference, invalid memory access), the kernel prints an **Oops**:

```text
BUG: unable to handle page fault for address: 0000000000000028
#PF: supervisor read access in kernel mode
#PF: error_code(0x0000) - not-present page
RIP: 0010:my_driver_read+0x24/0x80 [my_driver]
Code: ...
RSP: 0018:ffffb82a803f3da0 EFLAGS: 00010246
RAX: 0000000000000000 RBX: ffff8881023a5400 RCX: 0000000000000000
Call Trace:
 <TASK>
 vfs_read+0xac/0x310
 ksys_read+0x63/0xe0
 do_syscall_64+0x5b/0x90
 entry_SYSCALL_64_after_hwframe+0x76/0x7e
 </TASK>
```

### Decoding Steps:
1. **RIP (Instruction Pointer)**: Shows the function name and offset (`my_driver_read+0x24`).
2. **Translate with `addr2line`**:
   ```bash
   addr2line -e my_driver.ko 0x24
   ```
   This pinpoints the exact source code file and line number that triggered the fault!
3. **Analyze Registers**: `RAX: 0000000000000000` indicates a NULL pointer was dereferenced at offset `0x28`.

---

## 5. Summary & Best Practices

- **Avoid heavy computation inside Kprobe handlers**: Kprobe handlers execute with local interrupts disabled or in atomic contexts. Do not sleep, hold mutexes, or call blocking I/O functions.
- **Always unregister probes before unloading**: Unloading a module while active Kprobes are still planted causes immediate kernel panics upon the next invocation of the probed address.
- **Use atomic counters (`atomic64_t`)** for cross-core hit tracking to avoid costly spinlocks on high-frequency routines.
