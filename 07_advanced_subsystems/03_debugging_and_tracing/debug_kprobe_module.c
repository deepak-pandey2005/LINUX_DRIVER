// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file debug_kprobe_module.c
 * @brief Chapter 23: Kernel Debugging, Tracing & Kprobes (struct kprobe, dynamic instrumentation)
 * @author Deepak Pandey
 *
 * This module demonstrates:
 *  - Dynamic runtime kernel tracing using Kprobes without recompilation or reboots.
 *  - Hooking into target kernel routines (default: "do_sys_openat2") via struct kprobe.
 *  - Implementing pre_handler and post_handler callback routines.
 *  - Extracting execution context: struct pt_regs, instruction pointer (regs->ip), process PID & comm.
 *  - Atomic telemetry and race-free hit counters.
 *  - Exposing live trace metrics to user space via /proc/kprobe_stats.
 *  - Dynamic debug logging patterns (pr_debug, dev_dbg) and printk filtering.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kprobes.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/atomic.h>
#include <linux/sched.h>

#define PROC_ENTRY_NAME "kprobe_stats"
#define DEFAULT_TARGET  "do_sys_openat2"

static char *symbol_name = DEFAULT_TARGET;
module_param(symbol_name, charp, 0644);
MODULE_PARM_DESC(symbol_name, "Target kernel symbol to trace with Kprobe (default: do_sys_openat2)");

/* Telemetry counters */
static atomic64_t g_pre_hits = ATOMIC64_INIT(0);
static atomic64_t g_post_hits = ATOMIC64_INIT(0);

/* Last intercepted invocation telemetry */
static spinlock_t g_telemetry_lock;
static pid_t g_last_pid;
static char g_last_comm[TASK_COMM_LEN];
static unsigned long g_last_ip;

static struct kprobe kp;

/**
 * @brief Kprobe Pre-Handler: executes immediately before the probed instruction
 * @param p Pointer to the struct kprobe
 * @param regs Snapshot of processor registers at breakpoint
 * @return 0 to continue normal execution
 */
static int handler_pre(struct kprobe *p, struct pt_regs *regs)
{
    unsigned long flags;

    atomic64_inc(&g_pre_hits);

    spin_lock_irqsave(&g_telemetry_lock, flags);
    g_last_pid = current->pid;
    get_task_comm(g_last_comm, current);
#ifdef CONFIG_X86_64
    g_last_ip = regs->ip;
#else
    g_last_ip = instruction_pointer(regs);
#endif
    spin_unlock_irqrestore(&g_telemetry_lock, flags);

    pr_debug("Pre-handler: symbol '%s' hit by PID %d (%s) at IP 0x%lx\n",
             p->symbol_name, current->pid, current->comm, g_last_ip);

    return 0;
}

/**
 * @brief Kprobe Post-Handler: executes immediately after the probed instruction has run
 * @param p Pointer to the struct kprobe
 * @param regs Snapshot of processor registers
 * @param flags Architecture-specific CPU execution flags
 */
static void handler_post(struct kprobe *p, struct pt_regs *regs, unsigned long flags)
{
    atomic64_inc(&g_post_hits);
    pr_debug("Post-handler: symbol '%s' completed\n", p->symbol_name);
}

/**
 * @brief Procfs sequence file show callback
 */
static int kprobe_proc_show(struct seq_file *m, void *v)
{
    unsigned long flags;
    pid_t pid;
    char comm[TASK_COMM_LEN];
    unsigned long ip;

    spin_lock_irqsave(&g_telemetry_lock, flags);
    pid = g_last_pid;
    strscpy(comm, g_last_comm, sizeof(comm));
    ip = g_last_ip;
    spin_unlock_irqrestore(&g_telemetry_lock, flags);

    seq_printf(m, "========================================\n");
    seq_printf(m, "    Chapter 23: Kprobe Tracing Telemetry\n");
    seq_printf(m, "========================================\n");
    seq_printf(m, "Target Symbol:     %s\n", kp.symbol_name ? kp.symbol_name : "none");
    seq_printf(m, "Probe Address:     0x%px\n", kp.addr);
    seq_printf(m, "Pre-handler Hits:  %lld\n", (long long)atomic64_read(&g_pre_hits));
    seq_printf(m, "Post-handler Hits: %lld\n", (long long)atomic64_read(&g_post_hits));
    seq_printf(m, "Last Intercept PID: %d\n", pid);
    seq_printf(m, "Last Process Name: %s\n", comm[0] ? comm : "N/A");
    seq_printf(m, "Last Instruction:  0x%lx\n", ip);
    seq_printf(m, "========================================\n");

    return 0;
}

static int kprobe_proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, kprobe_proc_show, NULL);
}

static const struct proc_ops kprobe_proc_ops = {
    .proc_open    = kprobe_proc_open,
    .proc_read    = seq_read,
    .proc_lseek   = seq_lseek,
    .proc_release = single_release,
};

static int __init kprobe_module_init(void)
{
    int ret;

    pr_info("Initializing Chapter 23 Kernel Debugging & Tracing Module\n");
    spin_lock_init(&g_telemetry_lock);

    /* 1. Configure and register Kprobe */
    kp.symbol_name  = symbol_name;
    kp.pre_handler  = handler_pre;
    kp.post_handler = handler_post;

    ret = register_kprobe(&kp);
    if (ret < 0) {
        pr_err("register_kprobe failed for symbol '%s' (%d)\n", symbol_name, ret);
        return ret;
    }

    pr_info("Kprobe successfully planted on '%s' at address 0x%px\n",
            kp.symbol_name, kp.addr);

    /* 2. Create /proc entry for user telemetry */
    if (!proc_create(PROC_ENTRY_NAME, 0444, NULL, &kprobe_proc_ops)) {
        pr_err("Failed to create /proc/%s entry\n", PROC_ENTRY_NAME);
        unregister_kprobe(&kp);
        return -ENOMEM;
    }

    pr_info("Telemetry interface ready at /proc/%s\n", PROC_ENTRY_NAME);
    return 0;
}

static void __exit kprobe_module_exit(void)
{
    pr_info("Unloading Chapter 23 Kernel Debugging & Tracing Module\n");

    remove_proc_entry(PROC_ENTRY_NAME, NULL);
    unregister_kprobe(&kp);

    pr_info("Kprobe removed from 0x%px. Final stats: pre=%lld, post=%lld\n",
            kp.addr,
            (long long)atomic64_read(&g_pre_hits),
            (long long)atomic64_read(&g_post_hits));
}

module_init(kprobe_module_init);
module_exit(kprobe_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Deepak Pandey");
MODULE_DESCRIPTION("Chapter 23: Kernel Debugging, Dynamic Tracing & Kprobes");
MODULE_VERSION("1.0");
