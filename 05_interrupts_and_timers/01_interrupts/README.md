# Chapter 13: Hardware & Shared Interrupt Handling (request_irq, IRQF_SHARED, Top-Half ISR)

Welcome to Chapter 13 of the Linux Driver Development Curriculum. This lab covers how the Linux kernel interfaces with hardware interrupt controllers (APIC/GIC), registers Interrupt Service Routines (ISRs), handles shared IRQ lines, and executes top-half handlers safely.

---

## 1. Overview & Learning Objectives

When peripheral hardware needs CPU attention (e.g., packet received, key pressed, DMA transfer complete), it triggers a hardware interrupt. The CPU halts normal instruction flow and transfers control directly to the kernel's Interrupt Service Routine (ISR).

Key concepts mastered in this lab:
- Registering an interrupt handler with `request_irq()`.
- Shared interrupt architecture with `IRQF_SHARED` and the unique `dev_id` cookie.
- Top-Half vs Bottom-Half execution constraints.
- Why sleeping or allocating sleepable memory (`GFP_KERNEL`) is strictly forbidden in interrupt context.
- SMP-safe state tracking using `spin_lock_irqsave()`.
- Determining current core with `smp_processor_id()`.
- Inspecting active interrupt registrations via `/proc/interrupts`.
- Detaching interrupt handlers with `free_irq()`.

---

## 2. Directory Structure

```text
01_interrupts/
├── Makefile          # Kbuild compilation and automated test rules
├── interrupt_ioctl.h # IOCTL definitions for interrupt testing
├── interrupt_module.c# Kernel driver implementing shared ISR & simulation
├── test_client.c     # User-space verification suite
└── README.md         # This lab guide
```

---

## 3. Top-Half Rules of Engagement

| Operation | Allowed in Top-Half ISR? | Reason |
| :--- | :---: | :--- |
| `spin_lock_irqsave()` | ✅ YES | Busy-wait locking, disables local interrupts |
| `atomic_inc()` | ✅ YES | CPU bus-locked instruction, non-blocking |
| `ktime_get_ns()` | ✅ YES | Monotonic clock read, lockless |
| `mutex_lock()` | ❌ FORBIDDEN | Sleeps if contended -> Kernel panic |
| `copy_to/from_user()` | ❌ FORBIDDEN | May page-fault and sleep |
| `kmalloc(..., GFP_KERNEL)` | ❌ FORBIDDEN | May block waiting for free pages |
| `msleep()`, `schedule()` | ❌ FORBIDDEN | Cannot reschedule away from an IRQ |

---

## 4. Build, Load, and Test Instructions

### Step 1: Compile the Module and Test Client
```bash
make
```

### Step 2: Load the Module
```bash
sudo make load
```
Or with custom IRQ target:
```bash
sudo insmod interrupt_module.ko irq_num=1
ls -la /dev/irq_demo
```

### Step 3: Run the Verification Suite
```bash
make test
```
The test client verifies:
- Driver initialization and IRQ status query.
- Software interrupt burst triggering.
- Top-half ISR execution and multi-core CPU logging.
- Statistics reset.

### Step 4: Check `/proc/interrupts`
```bash
grep -i irq_demo /proc/interrupts
```

### Step 5: Unload the Module
```bash
sudo make unload
```

---

## 5. Architectural Guide

For the full theoretical breakdown of APIC/GIC architectures, interrupt priority, edge vs level triggering, and interrupt context vs process context, see [docs/13_interrupt_handling.md](file:///home/deepak/OPENSOURCE/LINUX_DRIVER/docs/13_interrupt_handling.md).

