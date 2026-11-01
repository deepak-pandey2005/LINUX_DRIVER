# Chapter 11: Concurrency, Synchronization & Locking in Linux Drivers

Welcome to Chapter 11 of the Linux Driver Development Curriculum. This lab covers concurrency, synchronization, and race condition elimination in multi-core SMP Linux systems.

---

## 1. Overview & Learning Objectives

Modern CPUs feature many physical cores executing kernel code in parallel. When multiple threads or interrupts access shared driver memory concurrently without synchronization, catastrophic data corruption, race conditions, and deadlocks occur.

Key concepts mastered in this lab:
- Understanding empirical race conditions caused by non-atomic Read-Modify-Write instructions (`counter++`).
- Lockless hardware atomic operations (`atomic64_t`, `atomic64_inc`, `atomic64_read`).
- Spinlocks (`spinlock_t`, `spin_lock`, `spin_unlock`) for fast, non-blocking critical sections.
- Mutexes (`struct mutex`, `mutex_lock_interruptible`, `mutex_unlock`) for sleepable process-context synchronization.
- Rules of engagement: what can be done in interrupt context vs process context.
- Benchmarking throughput and latency tradeoffs between lockless atomics, spinlocks, and mutexes.

---

## 2. Directory Structure

```text
01_locking/
├── Makefile          # Kbuild compilation and automated test rules
├── locking_ioctl.h   # Shared IOCTL command and payload definitions
├── locking_module.c  # Kernel module implementing concurrency benchmark
├── test_client.c     # Multi-threaded stress tester (POSIX threads)
└── README.md         # This lab guide
```

---

## 3. Synchronization Primitives Compared

| Primitive | Context | Sleep Allowed? | Implementation | Best Used For |
| :--- | :---: | :---: | :--- | :--- |
| **`atomic64_t`** | Any (Process & IRQ) | No | CPU bus-locked instruction / LL-SC | Simple counters, flags, reference counts |
| **`spinlock_t`** | Any (Process & IRQ) | ❌ NEVER | Active busy-wait loop on cacheline | Short, nanosecond critical sections, hardware registers |
| **`struct mutex`** | Process Context only | ✅ YES | Deschedules process until lock is free | Millisecond/long operations, allocating memory (`GFP_KERNEL`), I/O |

---

## 4. Build, Load, and Test Instructions

### Step 1: Compile the Module and Multi-Threaded Test Client
```bash
make
```

### Step 2: Load the Module
```bash
sudo make load
```
Or manually:
```bash
sudo insmod locking_module.ko
ls -la /dev/concurrency_bench
```

### Step 3: Run the Multi-Threaded Benchmark
```bash
make test
```
The test suite spawns **10 concurrent POSIX threads**, each hammering the driver with **20,000 increments** (Total 200,000 operations per test phase):
1. **Unprotected Mode**: Empirically demonstrates race conditions with lost updates (often 10%–30% lost increments!).
2. **Atomic Mode**: Validates 100% accuracy and measures lockless throughput.
3. **Spinlock Mode**: Validates 100% accuracy under busy-wait synchronization.
4. **Mutex Mode**: Validates 100% accuracy under sleepable synchronization.

### Step 4: Unload the Module
```bash
sudo make unload
```

---

## 5. Architectural Guide

For the full theoretical breakdown of SMP memory barriers, cache coherency (MESI), lockdep validator, deadlocks, and RCU (Read-Copy-Update), see [docs/11_concurrency_and_locking.md](file:///home/deepak/OPENSOURCE/LINUX_DRIVER/docs/11_concurrency_and_locking.md).

