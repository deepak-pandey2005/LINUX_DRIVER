# Chapter 12: Kernel Memory Management & Zero-Copy mmap Architecture

Welcome to Chapter 12 of the Linux Driver Development Curriculum. This lab covers physical memory allocation in the kernel, page management, and mapping device/kernel memory into user-space address spaces using the `mmap()` subsystem.

---

## 1. Overview & Learning Objectives

Standard kernel I/O mechanisms like `read()` and `write()` rely on `copy_to_user()` and `copy_from_user()`. For high-bandwidth applications (such as GPU buffers, packet captures, software-defined radio, audio interfaces, and industrial vision cameras), copying gigabytes of data between user and kernel buffers wastes CPU cycles and memory bandwidth.

Key concepts mastered in this lab:
- Physical vs virtual memory in the Linux kernel.
- Page allocation with `get_zeroed_page()` and `alloc_pages()`.
- Reserving pages (`SetPageReserved`) to prevent swapping and buddy allocator reclaim.
- Converting virtual addresses to Page Frame Numbers (`virt_to_page`, `page_to_pfn`).
- Implementing the VFS `file_operations.mmap` callback.
- Using `remap_pfn_range()` to map contiguous kernel pages directly into user VMA.
- Configuring VMA attributes with modern `vm_flags_set()` (`VM_DONTEXPAND`, `VM_DONTDUMP`).
- Zero-copy verification: direct pointer reads and writes between user space and kernel space.

---

## 2. Directory Structure

```text
02_memory_mmap/
├── Makefile          # Kbuild compilation and automated test rules
├── mmap_ioctl.h      # IOCTL commands for status verification
├── mmap_module.c     # Driver implementing zero-copy mmap
├── test_client.c     # User-space verification and bandwidth benchmark
└── README.md         # This lab guide
```

---

## 3. How Zero-Copy Memory Mapping Works

```text
+-------------------------------------------------------------+
|                     Physical RAM (Page)                     |
|           [ PFN: page_to_pfn(virt_to_page(kbuf)) ]          |
+-------------------------------------------------------------+
                     ^                       ^
                     | (1:1 Direct Map)      | (remap_pfn_range)
                     |                       |
+-----------------------------+     +-----------------------------+
|    Kernel Address Space     |     |   User Virtual Memory Area  |
|   (kbuf = get_zeroed_page)  |     |   (mapped_ptr = mmap(...))  |
+-----------------------------+     +-----------------------------+
```

Both kernel space (`kbuf`) and user space (`mapped_ptr`) point to the **same underlying physical DRAM page**. Any byte written to `mapped_ptr` is visible to `kbuf` instantly without CPU copy loops or system call context switching.

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
sudo insmod mmap_module.ko
ls -la /dev/zero_copy_mmap
```

### Step 3: Run the Verification and Benchmark Suite
```bash
make test
```
The test suite performs:
1. **Zero-Copy Read**: User application calls `mmap()` and reads the kernel banner directly by dereferencing `mapped_ptr` without invoking `read()`.
2. **Zero-Copy Write**: User application writes a message directly into `mapped_ptr`. Kernel inspects its internal memory and confirms receipt with 0 copies.
3. **Kernel Mutation Reflection**: Kernel fills the entire page with `0x5A` via an IOCTL trigger. User space verifies all 4096 bytes changed immediately.
4. **Bandwidth Benchmark**: Measures millions of operations per second via direct memory access.

### Step 4: Unload the Module
```bash
sudo make unload
```

---

## 5. Architectural Guide

For the full theoretical breakdown of the Linux Virtual Memory Subsystem, SLAB/SLUB allocators, highmem, caching modes (`pgprot_noncached`), and page tables, see [docs/12_kernel_memory_management.md](file:///home/deepak/OPENSOURCE/LINUX_DRIVER/docs/12_kernel_memory_management.md).
