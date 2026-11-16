// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file rtc_driver_module.c
 * @brief Chapter 32: Real-Time Clock (RTC) Subsystem Driver
 * @author Deepak Pandey
 *
 * This module demonstrates:
 *  - Linux RTC subsystem architecture and core layers.
 *  - Device registration with devm_rtc_allocate_device() & devm_rtc_register_device().
 *  - Struct rtc_class_ops implementation (read_time, set_time, read_alarm, set_alarm, alarm_irq_enable).
 *  - Hardware simulation: 32.768 kHz oscillator, battery backup, and alarm timer interrupt.
 *  - Firing alarm events via rtc_update_irq(rtc, 1, RTC_IRQF | RTC_AF).
 *  - Device wakeup capability integration (device_init_wakeup).
 *  - Integration with /dev/rtcX and /sys/class/rtc/rtcX/ sysfs interface.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/rtc.h>
#include <linux/timer.h>
#include <linux/spinlock.h>
#include <linux/timekeeping.h>
#include <linux/slab.h>

#define DRIVER_NAME "mock_rtc_driver"

struct mock_rtc_priv {
    struct platform_device *pdev;
    struct rtc_device *rtc;
    spinlock_t lock;

    /* Simulated Hardware RTC Registers */
    time64_t base_epoch_sec;     /* Base epoch set into hardware counter */
    uint64_t base_ktime_sec;     /* Monotonic kernel time when base was set */

    /* Alarm Registers */
    struct rtc_wkalrm alarm;
    struct timer_list alarm_timer;
    bool alarm_pending;

    /* Diagnostics & Telemetry */
    uint64_t alarm_fired_count;
    uint32_t battery_mv;         /* Simulated lithium coin cell 3000 mV */
    bool battery_ok;
};

static struct mock_rtc_priv *g_rtc_priv;

/**
 * @brief Calculate current simulated RTC hardware time in seconds since epoch.
 */
static time64_t mock_rtc_get_hw_seconds(struct mock_rtc_priv *priv)
{
    uint64_t now_ktime_sec = ktime_get_real_seconds();
    time64_t hw_time;
    unsigned long flags;

    spin_lock_irqsave(&priv->lock, flags);
    hw_time = priv->base_epoch_sec + (now_ktime_sec - priv->base_ktime_sec);
    spin_unlock_irqrestore(&priv->lock, flags);

    return hw_time;
}

/**
 * @brief Timer callback simulating hardware RTC alarm interrupt pin assertion.
 */
static void mock_rtc_alarm_timer_fn(struct timer_list *t)
{
    struct mock_rtc_priv *priv = container_of(t, struct mock_rtc_priv, alarm_timer);
    unsigned long flags;

    spin_lock_irqsave(&priv->lock, flags);
    priv->alarm_pending = true;
    priv->alarm_fired_count++;
    spin_unlock_irqrestore(&priv->lock, flags);

    dev_info(&priv->pdev->dev, "ALARM IRQ FIRED! Hardware alarm matched (Alarm event #%llu)\n",
             priv->alarm_fired_count);

    /* Signal RTC Core: 1 event, RTC_IRQF (interrupt occurred) | RTC_AF (alarm event) */
    rtc_update_irq(priv->rtc, 1, RTC_IRQF | RTC_AF);
}

/* =========================================================================
 * RTC Operations (struct rtc_class_ops)
 * ========================================================================= */

static int mock_rtc_read_time(struct device *dev, struct rtc_time *tm)
{
    struct mock_rtc_priv *priv = dev_get_drvdata(dev);
    time64_t secs;

    if (!priv)
        return -ENODEV;

    secs = mock_rtc_get_hw_seconds(priv);
    rtc_time64_to_tm(secs, tm);

    dev_dbg(dev, "read_time: %04d-%02d-%02d %02d:%02d:%02d (epoch=%lld)\n",
            tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
            tm->tm_hour, tm->tm_min, tm->tm_sec, (long long)secs);

    return 0;
}

static int mock_rtc_set_time(struct device *dev, struct rtc_time *tm)
{
    struct mock_rtc_priv *priv = dev_get_drvdata(dev);
    time64_t new_epoch_sec;
    unsigned long flags;

    if (!priv)
        return -ENODEV;

    new_epoch_sec = rtc_tm_to_time64(tm);

    spin_lock_irqsave(&priv->lock, flags);
    priv->base_epoch_sec = new_epoch_sec;
    priv->base_ktime_sec = ktime_get_real_seconds();
    spin_unlock_irqrestore(&priv->lock, flags);

    dev_info(dev, "set_time: Hardware updated to %04d-%02d-%02d %02d:%02d:%02d (epoch=%lld)\n",
             tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
             tm->tm_hour, tm->tm_min, tm->tm_sec, (long long)new_epoch_sec);

    return 0;
}

static int mock_rtc_read_alarm(struct device *dev, struct rtc_wkalrm *alrm)
{
    struct mock_rtc_priv *priv = dev_get_drvdata(dev);
    unsigned long flags;

    if (!priv)
        return -ENODEV;

    spin_lock_irqsave(&priv->lock, flags);
    *alrm = priv->alarm;
    alrm->pending = priv->alarm_pending ? 1 : 0;
    spin_unlock_irqrestore(&priv->lock, flags);

    return 0;
}

static int mock_rtc_set_alarm(struct device *dev, struct rtc_wkalrm *alrm)
{
    struct mock_rtc_priv *priv = dev_get_drvdata(dev);
    time64_t now_sec, alarm_sec, diff_sec;
    unsigned long flags;

    if (!priv)
        return -ENODEV;

    alarm_sec = rtc_tm_to_time64(&alrm->time);
    now_sec = mock_rtc_get_hw_seconds(priv);

    spin_lock_irqsave(&priv->lock, flags);
    priv->alarm = *alrm;
    priv->alarm_pending = false;

    timer_delete(&priv->alarm_timer);

    if (alrm->enabled) {
        diff_sec = alarm_sec - now_sec;
        if (diff_sec <= 0)
            diff_sec = 1; /* Trigger in 1 sec if requested time is in past/immediate */

        mod_timer(&priv->alarm_timer, jiffies + diff_sec * HZ);
        dev_info(dev, "Alarm armed for %04d-%02d-%02d %02d:%02d:%02d (in %lld sec)\n",
                 alrm->time.tm_year + 1900, alrm->time.tm_mon + 1, alrm->time.tm_mday,
                 alrm->time.tm_hour, alrm->time.tm_min, alrm->time.tm_sec,
                 (long long)diff_sec);
    } else {
        dev_info(dev, "Alarm disarmed\n");
    }
    spin_unlock_irqrestore(&priv->lock, flags);

    return 0;
}

static int mock_rtc_alarm_irq_enable(struct device *dev, unsigned int enabled)
{
    struct mock_rtc_priv *priv = dev_get_drvdata(dev);
    unsigned long flags;

    if (!priv)
        return -ENODEV;

    spin_lock_irqsave(&priv->lock, flags);
    priv->alarm.enabled = enabled ? 1 : 0;
    if (!enabled)
        timer_delete(&priv->alarm_timer);
    spin_unlock_irqrestore(&priv->lock, flags);

    dev_info(dev, "Alarm IRQ %s\n", enabled ? "enabled" : "disabled");
    return 0;
}

static const struct rtc_class_ops mock_rtc_ops = {
    .read_time        = mock_rtc_read_time,
    .set_time         = mock_rtc_set_time,
    .read_alarm       = mock_rtc_read_alarm,
    .set_alarm        = mock_rtc_set_alarm,
    .alarm_irq_enable = mock_rtc_alarm_irq_enable,
};

/* =========================================================================
 * Sysfs Diagnostics
 * ========================================================================= */

static ssize_t rtc_telemetry_show(struct device *dev,
                                  struct device_attribute *attr, char *buf)
{
    struct mock_rtc_priv *priv = dev_get_drvdata(dev);
    time64_t hw_sec;
    struct rtc_time tm;
    unsigned long flags;
    ssize_t ret;

    if (!priv)
        return -ENODEV;

    hw_sec = mock_rtc_get_hw_seconds(priv);
    rtc_time64_to_tm(hw_sec, &tm);

    spin_lock_irqsave(&priv->lock, flags);
    ret = scnprintf(buf, PAGE_SIZE,
                    "oscillator=32768Hz_QUARTZ\n"
                    "battery_voltage_mv=%u\n"
                    "battery_status=%s\n"
                    "alarm_fired_count=%llu\n"
                    "alarm_armed=%d\n"
                    "current_rtc_time=%04d-%02d-%02d %02d:%02d:%02d UTC\n",
                    priv->battery_mv,
                    priv->battery_ok ? "GOOD" : "DEPLETED",
                    priv->alarm_fired_count,
                    priv->alarm.enabled ? 1 : 0,
                    tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                    tm.tm_hour, tm.tm_min, tm.tm_sec);
    spin_unlock_irqrestore(&priv->lock, flags);

    return ret;
}
static DEVICE_ATTR_RO(rtc_telemetry);

static struct attribute *mock_rtc_attrs[] = {
    &dev_attr_rtc_telemetry.attr,
    NULL,
};
ATTRIBUTE_GROUPS(mock_rtc);

/* =========================================================================
 * Platform Driver Probe & Remove
 * ========================================================================= */

static int mock_rtc_probe(struct platform_device *pdev)
{
    struct mock_rtc_priv *priv;
    int ret;

    dev_info(&pdev->dev, "Probing Real-Time Clock (RTC) driver\n");

    priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
    if (!priv)
        return -ENOMEM;

    priv->pdev = pdev;
    spin_lock_init(&priv->lock);

    /* Initialize base time with current wall clock */
    priv->base_ktime_sec = ktime_get_real_seconds();
    priv->base_epoch_sec = priv->base_ktime_sec;
    priv->battery_mv = 3150; /* 3.15V healthy battery */
    priv->battery_ok = true;

    timer_setup(&priv->alarm_timer, mock_rtc_alarm_timer_fn, 0);

    platform_set_drvdata(pdev, priv);
    g_rtc_priv = priv;

    /* Allocate RTC Device */
    priv->rtc = devm_rtc_allocate_device(&pdev->dev);
    if (IS_ERR(priv->rtc)) {
        ret = PTR_ERR(priv->rtc);
        dev_err(&pdev->dev, "Failed to allocate RTC device: %d\n", ret);
        return ret;
    }

    priv->rtc->ops = &mock_rtc_ops;
    priv->rtc->range_min = RTC_TIMESTAMP_BEGIN_2000;
    priv->rtc->range_max = RTC_TIMESTAMP_END_2099;

    /* Register wakeup support */
    device_init_wakeup(&pdev->dev, true);

    /* Register RTC device */
    ret = devm_rtc_register_device(priv->rtc);
    if (ret < 0) {
        dev_err(&pdev->dev, "Failed to register RTC device: %d\n", ret);
        return ret;
    }

    dev_info(&pdev->dev, "RTC device registered as '%s' (range 2000-2099)\n",
             dev_name(&priv->rtc->dev));
    return 0;
}

static void mock_rtc_remove(struct platform_device *pdev)
{
    struct mock_rtc_priv *priv = platform_get_drvdata(pdev);

    dev_info(&pdev->dev, "Removing Real-Time Clock (RTC) driver\n");

    if (priv) {
        timer_shutdown_sync(&priv->alarm_timer);
        device_init_wakeup(&pdev->dev, false);
    }

    g_rtc_priv = NULL;
    dev_info(&pdev->dev, "RTC driver cleanup complete\n");
}

static struct platform_driver mock_rtc_platform_driver = {
    .probe  = mock_rtc_probe,
    .remove = mock_rtc_remove,
    .driver = {
        .name       = DRIVER_NAME,
        .dev_groups = mock_rtc_groups,
    },
};

static struct platform_device *mock_rtc_pdev;

/* =========================================================================
 * Module Init & Exit
 * ========================================================================= */

static int __init mock_rtc_init(void)
{
    int ret;

    pr_info("Loading Chapter 32: Real-Time Clock (RTC) Driver Module\n");

    mock_rtc_pdev = platform_device_register_simple(DRIVER_NAME, -1, NULL, 0);
    if (IS_ERR(mock_rtc_pdev)) {
        ret = PTR_ERR(mock_rtc_pdev);
        pr_err("Failed to register platform device: %d\n", ret);
        return ret;
    }

    ret = platform_driver_register(&mock_rtc_platform_driver);
    if (ret < 0) {
        pr_err("Failed to register platform driver: %d\n", ret);
        platform_device_unregister(mock_rtc_pdev);
        return ret;
    }

    pr_info("Chapter 32 module loaded successfully\n");
    return 0;
}

static void __exit mock_rtc_exit(void)
{
    pr_info("Unloading Chapter 32: Real-Time Clock (RTC) Driver Module\n");
    platform_driver_unregister(&mock_rtc_platform_driver);
    platform_device_unregister(mock_rtc_pdev);
    pr_info("Chapter 32 module unloaded successfully\n");
}

module_init(mock_rtc_init);
module_exit(mock_rtc_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Deepak Pandey");
MODULE_DESCRIPTION("Chapter 32: Real-Time Clock (RTC) Subsystem Driver");
MODULE_VERSION("1.0");
