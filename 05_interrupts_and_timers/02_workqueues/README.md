# Chapter 14: Deferred Work & Workqueues (schedule_work, alloc_workqueue, delayed_work)

Welcome to Chapter 14 of the Linux Driver Development Curriculum. This lab covers how the Linux kernel defers expensive computations and sleepable tasks from interrupt handlers to asynchronous kernel worker threads (**Workqueues**).

---

## 1. Overview & Learning Objectives

Top-Half ISRs must execute in minimal time and cannot sleep. To process packets, talk over I2C/SPI buses, or allocate memory, drivers defer work to a **Bottom Half**. The modern Linux kernel standard for sleepable deferred work is the **Workqueue subsystem**.

Key concepts mastered in this lab:
- Scheduling work on the default system workqueue (`system_wq`) using `INIT_WORK()` and `schedule_work()`.
- Allocating dedicated, private workqueues using `alloc_workqueue()` (`WQ_UNBOUND`).
- Scheduling timed deferred tasks with `struct delayed_work` and `schedule_delayed_work()`.
- Synchronously canceling in-flight work with `cancel_delayed_work_sync()`.
- Flushing and draining queues cleanly with `flush_workqueue()` and `destroy_workqueue()`.
- Verifying that work handlers execute in **process context** (where `in_interrupt() == false`, sleeping with `msleep()` is permitted, and `current->pid` corresponds to a `kworker` thread).

---

## 2. Directory Structure

```text
02_workqueues/
├── Makefile          # Kbuild compilation and automated test rules
├── workqueue_ioctl.h # IOCTL commands and statistics structures
├── workqueue_module.c# Kernel driver implementing workqueue variants
├── test_client.c     # User-space verification suite
└── README.md         # This lab guide
```

---

## 3. Workqueue Variants Compared

| Mechanism | Structure | Function | Execution Context | Sleep Allowed? |
| :--- | :--- | :--- | :---: | :---: |
| **System Workqueue** | `struct work_struct` | `schedule_work()` | `kworker` thread pool | ✅ YES |
| **Dedicated Workqueue**| `struct work_struct` | `queue_work(wq, ...)` | Dedicated worker threads | ✅ YES |
| **Delayed Work** | `struct delayed_work`| `schedule_delayed_work()`| Timer -> `kworker` | ✅ YES |

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
Or manually:
```bash
sudo insmod workqueue_module.ko
ls -la /dev/workqueue_demo
```

### Step 3: Run the Verification Suite
```bash
make test
```
The test client verifies:
- Queuing and running tasks on `system_wq`.
- Queuing and running tasks on a private custom workqueue.
- Scheduling delayed work with timeout expiration and confirmation.
- Synchronously canceling in-flight delayed work before it runs.

### Step 4: Unload the Module
```bash
sudo make unload
```

---

## 5. Architectural Guide

For the full theoretical breakdown of bottom halves (Softirqs vs Tasklets [legacy] vs Workqueues vs Threaded IRQs), concurrency models, and CMwq (Concurrency Managed Workqueues), see [docs/14_deferred_execution.md](file:///home/deepak/OPENSOURCE/LINUX_DRIVER/docs/14_deferred_execution.md).

