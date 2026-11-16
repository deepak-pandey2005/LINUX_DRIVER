// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file pm_driver_module.c
 * @brief Chapter 31: Device Power Management & Runtime PM Subsystem
 * @author Deepak Pandey
 *
 * This module demonstrates:
 *  - Linux Device Power Management (PM) architecture.
 *  - Struct dev_pm_ops integrating System Sleep (suspend/resume) and Runtime PM.
 *  - Dynamic device power state management with pm_runtime_get_sync() and pm_runtime_put_autosuspend().
 *  - Configurable autosuspend delay, idle detection, and last-busy markers.
 *  - Hardware simulation: Dynamic clock gating and power rail toggling.
 *  - Integration with user-space character device (/dev/pm_demo_dev) and sysfs telemetry.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/timekeeping.h>
#include <linux/ioctl.h>

#define DRIVER_NAME         "pm_demo_driver"
#define DEVICE_NAME         "pm_demo_dev"
#define DEFAULT_AUTOSUSPEND_MS 2000

/* IOCTL Definitions */
#define PM_DEMO_IOC_MAGIC 'P'

enum pm_demo_state {
    PM_DEMO_STATE_SUSPENDED = 0,
    PM_DEMO_STATE_ACTIVE    = 1,
    PM_DEMO_STATE_SLEEP     = 2,
};

struct pm_demo_stats {
    uint32_t power_state;      /* 0: Suspended, 1: Active, 2: Sleep */
    uint32_t usage_count;      /* Runtime PM usage reference counter */
    uint32_t autosuspend_ms;  /* Current autosuspend delay in ms */
    uint64_t resume_count;    /* Total runtime resume operations */
    uint64_t suspend_count;   /* Total runtime suspend operations */
    uint64_t active_time_ms;  /* Total time active in ms */
    uint8_t  hw_powered;      /* 1 = Power rails energized, 0 = Gated */
    uint8_t  clock_enabled;   /* 1 = Clock oscillator active, 0 = Stopped */
    uint8_t  reserved[6];
};

#define PM_DEMO_IOC_GET_STATS       _IOR(PM_DEMO_IOC_MAGIC, 1, struct pm_demo_stats)
#define PM_DEMO_IOC_SET_AUTOSUSPEND _IOW(PM_DEMO_IOC_MAGIC, 2, uint32_t)
#define PM_DEMO_IOC_WAKE_BURST      _IO(PM_DEMO_IOC_MAGIC, 3)

struct pm_demo_priv {
    struct platform_device *pdev;
    dev_t dev_num;
    struct cdev cdev;
    struct class *dev_class;
    struct device *char_dev;

    spinlock_t lock;
    enum pm_demo_state state;
    bool hw_powered;
    bool clock_enabled;
    uint32_t autosuspend_delay_ms;

    uint64_t resume_cycles;
    uint64_t suspend_cycles;
    uint64_t last_resume_ktime_ms;
    uint64_t total_active_ktime_ms;

    atomic_t force_active_ref;
};

static struct pm_demo_priv *g_pm_priv;

/* =========================================================================
 * Power Management Callbacks (struct dev_pm_ops)
 * ========================================================================= */

/**
 * @brief Runtime PM Suspend callback.
 * Invoked by the PM core when the usage counter is 0 and autosuspend delay expires.
 */
static int pm_demo_runtime_suspend(struct device *dev)
{
    struct pm_demo_priv *priv = dev_get_drvdata(dev);
    unsigned long flags;
    uint64_t now_ms;

    if (!priv)
        return 0;

    now_ms = ktime_to_ms(ktime_get());

    spin_lock_irqsave(&priv->lock, flags);
    if (priv->last_resume_ktime_ms > 0 && now_ms >= priv->last_resume_ktime_ms)
        priv->total_active_ktime_ms += (now_ms - priv->last_resume_ktime_ms);

    priv->hw_powered = false;
    priv->clock_enabled = false;
    priv->state = PM_DEMO_STATE_SUSPENDED;
    priv->suspend_cycles++;
    spin_unlock_irqrestore(&priv->lock, flags);

    dev_info(dev, "[RUNTIME PM] Suspend: Hardware rails gated, clock disabled (suspend #%llu)\n",
             priv->suspend_cycles);
    return 0;
}

/**
 * @brief Runtime PM Resume callback.
 * Invoked synchronously or asynchronously when pm_runtime_get* wakes the device.
 */
static int pm_demo_runtime_resume(struct device *dev)
{
    struct pm_demo_priv *priv = dev_get_drvdata(dev);
    unsigned long flags;

    if (!priv)
        return 0;

    spin_lock_irqsave(&priv->lock, flags);
    priv->hw_powered = true;
    priv->clock_enabled = true;
    priv->state = PM_DEMO_STATE_ACTIVE;
    priv->last_resume_ktime_ms = ktime_to_ms(ktime_get());
    priv->resume_cycles++;
    spin_unlock_irqrestore(&priv->lock, flags);

    dev_info(dev, "[RUNTIME PM] Resume: Hardware rails ON, clock stabilized (resume #%llu)\n",
             priv->resume_cycles);
    return 0;
}

/**
 * @brief Runtime PM Idle callback.
 * Invoked when usage count drops to 0. Return 0 to allow autosuspend timer to proceed.
 */
static int pm_demo_runtime_idle(struct device *dev)
{
    dev_dbg(dev, "[RUNTIME PM] Idle notification received: scheduling autosuspend\n");
    return 0;
}

/**
 * @brief System Sleep Suspend callback (S3/S4 entry).
 */
static int pm_demo_system_suspend(struct device *dev)
{
    struct pm_demo_priv *priv = dev_get_drvdata(dev);
    unsigned long flags;

    if (!priv)
        return 0;

    spin_lock_irqsave(&priv->lock, flags);
    priv->hw_powered = false;
    priv->clock_enabled = false;
    priv->state = PM_DEMO_STATE_SLEEP;
    spin_unlock_irqrestore(&priv->lock, flags);

    dev_info(dev, "[SYSTEM PM] Suspend invoked: Device placed into deep system sleep\n");
    return 0;
}

/**
 * @brief System Sleep Resume callback (S3/S4 exit).
 */
static int pm_demo_system_resume(struct device *dev)
{
    struct pm_demo_priv *priv = dev_get_drvdata(dev);
    unsigned long flags;

    if (!priv)
        return 0;

    spin_lock_irqsave(&priv->lock, flags);
    priv->hw_powered = true;
    priv->clock_enabled = true;
    priv->state = PM_DEMO_STATE_ACTIVE;
    priv->last_resume_ktime_ms = ktime_to_ms(ktime_get());
    spin_unlock_irqrestore(&priv->lock, flags);

    dev_info(dev, "[SYSTEM PM] Resume invoked: Device restored from deep system sleep\n");
    return 0;
}

static const struct dev_pm_ops pm_demo_dev_pm_ops = {
    SYSTEM_SLEEP_PM_OPS(pm_demo_system_suspend, pm_demo_system_resume)
    RUNTIME_PM_OPS(pm_demo_runtime_suspend, pm_demo_runtime_resume, pm_demo_runtime_idle)
};

/* =========================================================================
 * Character Device File Operations
 * ========================================================================= */

static int pm_demo_fops_open(struct inode *inode, struct file *file)
{
    struct pm_demo_priv *priv = container_of(inode->i_cdev, struct pm_demo_priv, cdev);
    int ret;

    file->private_data = priv;

    /* Wake up hardware synchronously on device node open */
    ret = pm_runtime_get_sync(&priv->pdev->dev);
    if (ret < 0) {
        pm_runtime_put_noidle(&priv->pdev->dev);
        pr_err("Failed to resume device in open(): %d\n", ret);
        return ret;
    }

    dev_info(&priv->pdev->dev, "Char device opened: usage_count=%d\n",
             atomic_read(&priv->pdev->dev.power.usage_count));
    return 0;
}

static int pm_demo_fops_release(struct inode *inode, struct file *file)
{
    struct pm_demo_priv *priv = file->private_data;

    if (!priv)
        return 0;

    /* Mark device as last busy and drop reference with autosuspend delay */
    pm_runtime_mark_last_busy(&priv->pdev->dev);
    pm_runtime_put_autosuspend(&priv->pdev->dev);

    dev_info(&priv->pdev->dev, "Char device released: autosuspend scheduled (delay=%d ms)\n",
             priv->autosuspend_delay_ms);
    return 0;
}

static ssize_t pm_demo_fops_read(struct file *file, char __user *user_buf,
                                 size_t count, loff_t *ppos)
{
    struct pm_demo_priv *priv = file->private_data;
    char buffer[256];
    int len;
    unsigned long flags;
    const char *state_str = "UNKNOWN";

    if (*ppos > 0)
        return 0;

    /* Refresh busy timer */
    pm_runtime_mark_last_busy(&priv->pdev->dev);

    spin_lock_irqsave(&priv->lock, flags);
    switch (priv->state) {
    case PM_DEMO_STATE_SUSPENDED: state_str = "SUSPENDED (Gated)"; break;
    case PM_DEMO_STATE_ACTIVE:    state_str = "ACTIVE (Running)"; break;
    case PM_DEMO_STATE_SLEEP:     state_str = "SYSTEM_SLEEP (S3/S4)"; break;
    }

    len = scnprintf(buffer, sizeof(buffer),
                    "PM Demo Telemetry:\n"
                    "  State:          %s\n"
                    "  Power Rails:    %s\n"
                    "  Clock:          %s\n"
                    "  Usage Count:    %d\n"
                    "  Resume Cycles:  %llu\n"
                    "  Suspend Cycles: %llu\n"
                    "  Autosuspend:    %d ms\n",
                    state_str,
                    priv->hw_powered ? "ON (3.3V / 1.2V Core)" : "OFF (0.0V)",
                    priv->clock_enabled ? "ON (100 MHz PLL Active)" : "OFF (Gated)",
                    atomic_read(&priv->pdev->dev.power.usage_count),
                    priv->resume_cycles,
                    priv->suspend_cycles,
                    priv->autosuspend_delay_ms);
    spin_unlock_irqrestore(&priv->lock, flags);

    if (count < len)
        len = count;

    if (copy_to_user(user_buf, buffer, len))
        return -EFAULT;

    *ppos += len;
    return len;
}

static ssize_t pm_demo_fops_write(struct file *file, const char __user *user_buf,
                                  size_t count, loff_t *ppos)
{
    struct pm_demo_priv *priv = file->private_data;
    char kbuf[64];
    size_t copy_size = min(count, sizeof(kbuf) - 1);

    if (copy_from_user(kbuf, user_buf, copy_size))
        return -EFAULT;

    kbuf[copy_size] = '\0';

    /* Mark busy on activity */
    pm_runtime_mark_last_busy(&priv->pdev->dev);
    dev_info(&priv->pdev->dev, "Simulated hardware transaction executed: %zu bytes written\n", count);

    return count;
}

static long pm_demo_fops_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    struct pm_demo_priv *priv = file->private_data;
    struct pm_demo_stats stats;
    unsigned long flags;
    uint32_t delay_val;
    uint64_t now_ms;

    switch (cmd) {
    case PM_DEMO_IOC_GET_STATS:
        memset(&stats, 0, sizeof(stats));
        now_ms = ktime_to_ms(ktime_get());

        spin_lock_irqsave(&priv->lock, flags);
        stats.power_state    = priv->state;
        stats.usage_count    = atomic_read(&priv->pdev->dev.power.usage_count);
        stats.autosuspend_ms = priv->autosuspend_delay_ms;
        stats.resume_count   = priv->resume_cycles;
        stats.suspend_count  = priv->suspend_cycles;
        stats.hw_powered     = priv->hw_powered ? 1 : 0;
        stats.clock_enabled  = priv->clock_enabled ? 1 : 0;

        if (priv->state == PM_DEMO_STATE_ACTIVE && priv->last_resume_ktime_ms > 0)
            stats.active_time_ms = priv->total_active_ktime_ms + (now_ms - priv->last_resume_ktime_ms);
        else
            stats.active_time_ms = priv->total_active_ktime_ms;
        spin_unlock_irqrestore(&priv->lock, flags);

        if (copy_to_user((void __user *)arg, &stats, sizeof(stats)))
            return -EFAULT;
        return 0;

    case PM_DEMO_IOC_SET_AUTOSUSPEND:
        if (copy_from_user(&delay_val, (void __user *)arg, sizeof(delay_val)))
            return -EFAULT;

        if (delay_val > 60000)
            return -EINVAL; /* Max 60 seconds */

        spin_lock_irqsave(&priv->lock, flags);
        priv->autosuspend_delay_ms = delay_val;
        spin_unlock_irqrestore(&priv->lock, flags);

        pm_runtime_set_autosuspend_delay(&priv->pdev->dev, delay_val);
        dev_info(&priv->pdev->dev, "Updated autosuspend delay to %u ms\n", delay_val);
        return 0;

    case PM_DEMO_IOC_WAKE_BURST:
        pm_runtime_mark_last_busy(&priv->pdev->dev);
        dev_info(&priv->pdev->dev, "Activity burst acknowledged: timer reset\n");
        return 0;

    default:
        return -ENOTTY;
    }
}

static const struct file_operations pm_demo_fops = {
    .owner          = THIS_MODULE,
    .open           = pm_demo_fops_open,
    .release        = pm_demo_fops_release,
    .read           = pm_demo_fops_read,
    .write          = pm_demo_fops_write,
    .unlocked_ioctl = pm_demo_fops_ioctl,
};

/* =========================================================================
 * Sysfs Attributes
 * ========================================================================= */

static ssize_t power_telemetry_show(struct device *dev,
                                    struct device_attribute *attr, char *buf)
{
    struct pm_demo_priv *priv = dev_get_drvdata(dev);
    unsigned long flags;
    ssize_t ret;
    const char *state_str;
    uint32_t power_mw;

    if (!priv)
        return -ENODEV;

    spin_lock_irqsave(&priv->lock, flags);
    switch (priv->state) {
    case PM_DEMO_STATE_SUSPENDED:
        state_str = "SUSPENDED";
        power_mw = 12; /* 12 mW quiescent sleep current */
        break;
    case PM_DEMO_STATE_ACTIVE:
        state_str = "ACTIVE";
        power_mw = 480; /* 480 mW dynamic power */
        break;
    default:
        state_str = "SLEEP";
        power_mw = 1; /* 1 mW deep retention */
        break;
    }

    ret = scnprintf(buf, PAGE_SIZE,
                    "state=%s\n"
                    "power_dissipation_mw=%u\n"
                    "usage_count=%d\n"
                    "resumes=%llu\n"
                    "suspends=%llu\n"
                    "autosuspend_delay_ms=%d\n"
                    "rails_energized=%d\n"
                    "clock_enabled=%d\n",
                    state_str,
                    power_mw,
                    atomic_read(&dev->power.usage_count),
                    priv->resume_cycles,
                    priv->suspend_cycles,
                    priv->autosuspend_delay_ms,
                    priv->hw_powered ? 1 : 0,
                    priv->clock_enabled ? 1 : 0);
    spin_unlock_irqrestore(&priv->lock, flags);

    return ret;
}
static DEVICE_ATTR_RO(power_telemetry);

static ssize_t force_active_show(struct device *dev,
                                 struct device_attribute *attr, char *buf)
{
    struct pm_demo_priv *priv = dev_get_drvdata(dev);

    return scnprintf(buf, PAGE_SIZE, "%d\n", atomic_read(&priv->force_active_ref));
}

static ssize_t force_active_store(struct device *dev,
                                  struct device_attribute *attr,
                                  const char *buf, size_t count)
{
    struct pm_demo_priv *priv = dev_get_drvdata(dev);
    int val;

    if (kstrtoint(buf, 10, &val))
        return -EINVAL;

    if (val == 1) {
        if (atomic_inc_return(&priv->force_active_ref) == 1) {
            pm_runtime_get_sync(dev);
            dev_info(dev, "Sysfs force_active engaged: Device held in ACTIVE state\n");
        }
    } else if (val == 0) {
        if (atomic_read(&priv->force_active_ref) > 0) {
            atomic_dec(&priv->force_active_ref);
            pm_runtime_mark_last_busy(dev);
            pm_runtime_put_autosuspend(dev);
            dev_info(dev, "Sysfs force_active disengaged: Autosuspend enabled\n");
        }
    } else {
        return -EINVAL;
    }

    return count;
}
static DEVICE_ATTR_RW(force_active);

static struct attribute *pm_demo_attrs[] = {
    &dev_attr_power_telemetry.attr,
    &dev_attr_force_active.attr,
    NULL,
};
ATTRIBUTE_GROUPS(pm_demo);

/* =========================================================================
 * Platform Driver Probe & Remove
 * ========================================================================= */

static int pm_demo_probe(struct platform_device *pdev)
{
    struct pm_demo_priv *priv;
    int ret;

    dev_info(&pdev->dev, "Probing Power Management demo driver\n");

    priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
    if (!priv)
        return -ENOMEM;

    priv->pdev = pdev;
    priv->autosuspend_delay_ms = DEFAULT_AUTOSUSPEND_MS;
    priv->state = PM_DEMO_STATE_ACTIVE;
    priv->hw_powered = true;
    priv->clock_enabled = true;
    priv->last_resume_ktime_ms = ktime_to_ms(ktime_get());
    spin_lock_init(&priv->lock);
    atomic_set(&priv->force_active_ref, 0);

    platform_set_drvdata(pdev, priv);
    g_pm_priv = priv;

    /* Allocate Character Device region */
    ret = alloc_chrdev_region(&priv->dev_num, 0, 1, DEVICE_NAME);
    if (ret < 0) {
        dev_err(&pdev->dev, "alloc_chrdev_region failed: %d\n", ret);
        return ret;
    }

    cdev_init(&priv->cdev, &pm_demo_fops);
    priv->cdev.owner = THIS_MODULE;
    ret = cdev_add(&priv->cdev, priv->dev_num, 1);
    if (ret < 0) {
        dev_err(&pdev->dev, "cdev_add failed: %d\n", ret);
        goto err_unregister_chrdev;
    }

    priv->dev_class = class_create(DEVICE_NAME);
    if (IS_ERR(priv->dev_class)) {
        ret = PTR_ERR(priv->dev_class);
        dev_err(&pdev->dev, "class_create failed: %d\n", ret);
        goto err_cdev_del;
    }

    priv->char_dev = device_create(priv->dev_class, &pdev->dev, priv->dev_num, NULL, DEVICE_NAME);
    if (IS_ERR(priv->char_dev)) {
        ret = PTR_ERR(priv->char_dev);
        dev_err(&pdev->dev, "device_create failed: %d\n", ret);
        goto err_class_destroy;
    }

    /* Configure Runtime PM */
    pm_runtime_set_autosuspend_delay(&pdev->dev, priv->autosuspend_delay_ms);
    pm_runtime_use_autosuspend(&pdev->dev);
    pm_runtime_set_active(&pdev->dev);
    pm_runtime_enable(&pdev->dev);
    pm_runtime_allow(&pdev->dev);

    /* Mark busy and release initial probe hold so autosuspend can trigger */
    pm_runtime_mark_last_busy(&pdev->dev);
    pm_runtime_put_autosuspend(&pdev->dev);

    dev_info(&pdev->dev, "PM Demo Driver successfully initialized (/dev/%s)\n", DEVICE_NAME);
    return 0;

err_class_destroy:
    class_destroy(priv->dev_class);
err_cdev_del:
    cdev_del(&priv->cdev);
err_unregister_chrdev:
    unregister_chrdev_region(priv->dev_num, 1);
    return ret;
}

static void pm_demo_remove(struct platform_device *pdev)
{
    struct pm_demo_priv *priv = platform_get_drvdata(pdev);

    dev_info(&pdev->dev, "Removing Power Management demo driver\n");

    if (!priv)
        return;

    /* Teardown Runtime PM */
    pm_runtime_forbid(&pdev->dev);
    pm_runtime_disable(&pdev->dev);
    pm_runtime_dont_use_autosuspend(&pdev->dev);

    device_destroy(priv->dev_class, priv->dev_num);
    class_destroy(priv->dev_class);
    cdev_del(&priv->cdev);
    unregister_chrdev_region(priv->dev_num, 1);

    g_pm_priv = NULL;
    dev_info(&pdev->dev, "PM Demo Driver cleanup complete\n");
}

static struct platform_driver pm_demo_driver = {
    .probe  = pm_demo_probe,
    .remove = pm_demo_remove,
    .driver = {
        .name           = DRIVER_NAME,
        .pm             = &pm_demo_dev_pm_ops,
        .dev_groups     = pm_demo_groups,
    },
};

static struct platform_device *pm_demo_pdev;

/* =========================================================================
 * Module Init & Exit
 * ========================================================================= */

static int __init pm_demo_module_init(void)
{
    int ret;

    pr_info("Loading Chapter 31: Device Power Management & Runtime PM Module\n");

    /* Register mock platform device */
    pm_demo_pdev = platform_device_register_simple(DRIVER_NAME, -1, NULL, 0);
    if (IS_ERR(pm_demo_pdev)) {
        ret = PTR_ERR(pm_demo_pdev);
        pr_err("Failed to register platform device: %d\n", ret);
        return ret;
    }

    /* Register platform driver */
    ret = platform_driver_register(&pm_demo_driver);
    if (ret < 0) {
        pr_err("Failed to register platform driver: %d\n", ret);
        platform_device_unregister(pm_demo_pdev);
        return ret;
    }

    pr_info("Chapter 31 module loaded successfully\n");
    return 0;
}

static void __exit pm_demo_module_exit(void)
{
    pr_info("Unloading Chapter 31: Device Power Management & Runtime PM Module\n");
    platform_driver_unregister(&pm_demo_driver);
    platform_device_unregister(pm_demo_pdev);
    pr_info("Chapter 31 module unloaded successfully\n");
}

module_init(pm_demo_module_init);
module_exit(pm_demo_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Deepak Pandey");
MODULE_DESCRIPTION("Chapter 31: Device Power Management & Runtime PM Subsystem");
MODULE_VERSION("1.0");
