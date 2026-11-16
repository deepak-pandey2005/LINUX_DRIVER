// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file watchdog_driver_module.c
 * @brief Chapter 27: Watchdog Timer Subsystem (struct watchdog_device, watchdog_ops)
 * @author Deepak Pandey
 *
 * This module demonstrates:
 *  - Linux Watchdog Subsystem architecture (watchdog_device, watchdog_ops, watchdog_info).
 *  - Watchdog options: WDIOF_SETTIMEOUT, WDIOF_KEEPALIVEPING, WDIOF_MAGICCLOSE.
 *  - Implementing callbacks: start(), stop(), ping(), set_timeout(), get_timeleft().
 *  - In-kernel software countdown timer simulating hardware watchdog heartbeat.
 *  - User-space interaction via standard /dev/watchdog and ioctl(WDIOC_KEEPALIVE, WDIOC_SETTIMEOUT).
 *  - Handling the Magic Close pattern ('V') to distinguish intentional shutdown from failure.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/watchdog.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/platform_device.h>

#define DRIVER_NAME       "mock_watchdog"
#define DEFAULT_TIMEOUT   10  /* 10 seconds */
#define MIN_TIMEOUT       1
#define MAX_TIMEOUT       60

struct mock_wdt_priv {
    struct watchdog_device wdd;
    struct timer_list timer;
    spinlock_t lock;

    unsigned long expires_at;
    bool running;
    u64 ping_count;
    u64 expiration_count;
};

static struct mock_wdt_priv *g_wdt;

/**
 * @brief Software countdown timer callback: simulates hardware reset trigger
 */
static void mock_wdt_timer_fn(struct timer_list *t)
{
    struct mock_wdt_priv *priv = container_of(t, struct mock_wdt_priv, timer);
    unsigned long flags;

    spin_lock_irqsave(&priv->lock, flags);
    if (priv->running) {
        priv->expiration_count++;
        pr_emerg("************************************************************\n");
        pr_emerg("  WATCHDOG EXPIRED! System hung or user process crashed!   \n");
        pr_emerg("  (In physical hardware, PMIC/SoC would reset the machine) \n");
        pr_emerg("************************************************************\n");

        /* Rearm to prevent spamming logs */
        priv->expires_at = jiffies + (priv->wdd.timeout * HZ);
        mod_timer(&priv->timer, priv->expires_at);
    }
    spin_unlock_irqrestore(&priv->lock, flags);
}

static int mock_wdt_start(struct watchdog_device *wdd)
{
    struct mock_wdt_priv *priv = watchdog_get_drvdata(wdd);
    unsigned long flags;

    spin_lock_irqsave(&priv->lock, flags);
    priv->running = true;
    priv->expires_at = jiffies + (wdd->timeout * HZ);
    mod_timer(&priv->timer, priv->expires_at);
    spin_unlock_irqrestore(&priv->lock, flags);

    pr_info("Watchdog started! Timeout set to %u seconds\n", wdd->timeout);
    return 0;
}

static int mock_wdt_stop(struct watchdog_device *wdd)
{
    struct mock_wdt_priv *priv = watchdog_get_drvdata(wdd);
    unsigned long flags;

    spin_lock_irqsave(&priv->lock, flags);
    priv->running = false;
    timer_delete(&priv->timer);
    spin_unlock_irqrestore(&priv->lock, flags);

    pr_info("Watchdog stopped cleanly\n");
    return 0;
}

static int mock_wdt_ping(struct watchdog_device *wdd)
{
    struct mock_wdt_priv *priv = watchdog_get_drvdata(wdd);
    unsigned long flags;

    spin_lock_irqsave(&priv->lock, flags);
    if (priv->running) {
        priv->ping_count++;
        priv->expires_at = jiffies + (wdd->timeout * HZ);
        mod_timer(&priv->timer, priv->expires_at);
        pr_debug("Watchdog petted (ping #%llu), reset deadline to +%u sec\n",
                 priv->ping_count, wdd->timeout);
    }
    spin_unlock_irqrestore(&priv->lock, flags);

    return 0;
}

static int mock_wdt_set_timeout(struct watchdog_device *wdd, unsigned int timeout)
{
    struct mock_wdt_priv *priv = watchdog_get_drvdata(wdd);
    unsigned long flags;

    wdd->timeout = timeout;

    spin_lock_irqsave(&priv->lock, flags);
    if (priv->running) {
        priv->expires_at = jiffies + (wdd->timeout * HZ);
        mod_timer(&priv->timer, priv->expires_at);
    }
    spin_unlock_irqrestore(&priv->lock, flags);

    pr_info("Watchdog timeout changed to %u seconds\n", timeout);
    return 0;
}

static unsigned int mock_wdt_get_timeleft(struct watchdog_device *wdd)
{
    struct mock_wdt_priv *priv = watchdog_get_drvdata(wdd);
    unsigned long flags;
    unsigned long now = jiffies;
    unsigned int timeleft = 0;

    spin_lock_irqsave(&priv->lock, flags);
    if (priv->running && time_after(priv->expires_at, now)) {
        timeleft = (priv->expires_at - now) / HZ;
    }
    spin_unlock_irqrestore(&priv->lock, flags);

    return timeleft;
}

static const struct watchdog_info mock_wdt_info = {
    .options  = WDIOF_SETTIMEOUT | WDIOF_KEEPALIVEPING | WDIOF_MAGICCLOSE,
    .identity = "Mock Virtual Watchdog",
    .firmware_version = 1,
};

static const struct watchdog_ops mock_wdt_ops = {
    .owner        = THIS_MODULE,
    .start        = mock_wdt_start,
    .stop         = mock_wdt_stop,
    .ping         = mock_wdt_ping,
    .set_timeout  = mock_wdt_set_timeout,
    .get_timeleft = mock_wdt_get_timeleft,
};

/* Sysfs Statistics Attribute */
static ssize_t wdt_stats_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct mock_wdt_priv *priv = g_wdt;

    if (!priv)
        return -ENODEV;

    return sysfs_emit(buf,
                      "Running:           %s\n"
                      "Timeout:           %u seconds\n"
                      "Time Left:         %u seconds\n"
                      "Total Pings:       %llu\n"
                      "Total Expirations: %llu\n",
                      priv->running ? "YES" : "NO",
                      priv->wdd.timeout,
                      mock_wdt_get_timeleft(&priv->wdd),
                      priv->ping_count,
                      priv->expiration_count);
}
static DEVICE_ATTR_RO(wdt_stats);

static struct platform_device *g_wdt_pdev;

static int mock_wdt_probe(struct platform_device *pdev)
{
    struct mock_wdt_priv *priv;
    int ret;

    pr_info("Probing Chapter 27 Watchdog Subsystem Device\n");

    priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
    if (!priv)
        return -ENOMEM;

    spin_lock_init(&priv->lock);
    timer_setup(&priv->timer, mock_wdt_timer_fn, 0);

    priv->wdd.info         = &mock_wdt_info;
    priv->wdd.ops          = &mock_wdt_ops;
    priv->wdd.min_timeout  = MIN_TIMEOUT;
    priv->wdd.max_timeout  = MAX_TIMEOUT;
    priv->wdd.timeout      = DEFAULT_TIMEOUT;
    priv->wdd.parent       = &pdev->dev;

    watchdog_set_drvdata(&priv->wdd, priv);

    ret = devm_watchdog_register_device(&pdev->dev, &priv->wdd);
    if (ret) {
        pr_err("devm_watchdog_register_device failed (%d)\n", ret);
        return ret;
    }

    ret = device_create_file(&pdev->dev, &dev_attr_wdt_stats);
    if (ret)
        pr_warn("device_create_file failed (%d)\n", ret);

    platform_set_drvdata(pdev, priv);
    g_wdt = priv;

    pr_info("Watchdog registered successfully! Node: /dev/watchdog\n");
    return 0;
}

static void mock_wdt_remove(struct platform_device *pdev)
{
    struct mock_wdt_priv *priv = platform_get_drvdata(pdev);

    pr_info("Removing Chapter 27 Watchdog Device\n");

    if (priv) {
        device_remove_file(&pdev->dev, &dev_attr_wdt_stats);
        timer_shutdown_sync(&priv->timer);
        g_wdt = NULL;
    }
}

static struct platform_driver mock_wdt_driver = {
    .driver = {
        .name = DRIVER_NAME,
        .owner = THIS_MODULE,
    },
    .probe  = mock_wdt_probe,
    .remove = mock_wdt_remove,
};

static int __init mock_wdt_init(void)
{
    int ret;

    pr_info("Initializing Chapter 27 Watchdog Subsystem Module\n");

    ret = platform_driver_register(&mock_wdt_driver);
    if (ret)
        return ret;

    g_wdt_pdev = platform_device_register_simple(DRIVER_NAME, -1, NULL, 0);
    if (IS_ERR(g_wdt_pdev)) {
        ret = PTR_ERR(g_wdt_pdev);
        platform_driver_unregister(&mock_wdt_driver);
        return ret;
    }

    return 0;
}

static void __exit mock_wdt_exit(void)
{
    pr_info("Unloading Chapter 27 Watchdog Subsystem Module\n");

    if (g_wdt_pdev)
        platform_device_unregister(g_wdt_pdev);

    platform_driver_unregister(&mock_wdt_driver);
    pr_info("Watchdog module unloaded cleanly\n");
}

module_init(mock_wdt_init);
module_exit(mock_wdt_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Deepak Pandey");
MODULE_DESCRIPTION("Chapter 27: Watchdog Timer Subsystem & Heartbeat Daemon Driver");
MODULE_VERSION("1.0");
