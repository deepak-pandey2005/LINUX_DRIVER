// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file pwm_driver_module.c
 * @brief Chapter 28: Linux PWM (Pulse Width Modulation) Subsystem
 * @author Deepak Pandey
 *
 * This module demonstrates:
 *  - Architecture of the Linux PWM framework (struct pwm_chip, struct pwm_ops).
 *  - Modern atomic state transition model: pwm_state, period, duty_cycle, polarity.
 *  - Implementing the mandatory ops: .apply() and .get_state().
 *  - Channel validation and frequency calculations in nanoseconds.
 *  - User-space control via /sys/class/pwm/pwmchipX/ (export, period, duty_cycle, enable).
 *  - Multi-channel simulated hardware PWM controller (2 independent channels).
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/pwm.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/mutex.h>

#define DRIVER_NAME  "mock_pwm_chip"
#define NUM_PWMS     2

struct mock_pwm_channel {
    struct pwm_state state;
    u64 apply_count;
};

struct mock_pwm_priv {
    struct mutex lock;
    struct mock_pwm_channel channels[NUM_PWMS];
    u64 total_config_changes;
};

static struct mock_pwm_priv *g_pwm_priv;

/**
 * @brief Atomically applies a new PWM configuration to hardware
 */
static int mock_pwm_apply(struct pwm_chip *chip, struct pwm_device *pwm,
                          const struct pwm_state *state)
{
    struct mock_pwm_priv *priv = pwmchip_get_drvdata(chip);
    unsigned int hwpwm = pwm->hwpwm;
    u64 duty_percent = 0;

    if (hwpwm >= NUM_PWMS)
        return -EINVAL;

    /* Validate: duty_cycle cannot exceed period */
    if (state->duty_cycle > state->period) {
        pr_err("PWM ch %u error: duty_cycle (%llu ns) > period (%llu ns)\n",
               hwpwm, state->duty_cycle, state->period);
        return -EINVAL;
    }

    if (state->period > 0)
        duty_percent = (state->duty_cycle * 100ULL) / state->period;

    mutex_lock(&priv->lock);
    priv->channels[hwpwm].state = *state;
    priv->channels[hwpwm].apply_count++;
    priv->total_config_changes++;
    mutex_unlock(&priv->lock);

    pr_info("PWM Channel %u updated: Period=%llu ns, Duty=%llu ns (%llu%%), Polarity=%s, Enabled=%s\n",
            hwpwm,
            state->period,
            state->duty_cycle,
            duty_percent,
            state->polarity == PWM_POLARITY_INVERSED ? "inversed" : "normal",
            state->enabled ? "YES" : "NO");

    return 0;
}

/**
 * @brief Queries current hardware configuration
 */
static int mock_pwm_get_state(struct pwm_chip *chip, struct pwm_device *pwm,
                              struct pwm_state *state)
{
    struct mock_pwm_priv *priv = pwmchip_get_drvdata(chip);
    unsigned int hwpwm = pwm->hwpwm;

    if (hwpwm >= NUM_PWMS)
        return -EINVAL;

    mutex_lock(&priv->lock);
    *state = priv->channels[hwpwm].state;
    mutex_unlock(&priv->lock);

    return 0;
}

static const struct pwm_ops mock_pwm_ops = {
    .apply     = mock_pwm_apply,
    .get_state = mock_pwm_get_state,
};

/* Sysfs Statistics Attribute */
static ssize_t pwm_stats_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct mock_pwm_priv *priv = g_pwm_priv;
    ssize_t len = 0;
    int i;

    if (!priv)
        return -ENODEV;

    mutex_lock(&priv->lock);
    len += sysfs_emit_at(buf, len, "Total Configuration Updates: %llu\n", priv->total_config_changes);

    for (i = 0; i < NUM_PWMS; ++i) {
        struct pwm_state *s = &priv->channels[i].state;
        u64 freq_hz = s->period > 0 ? (1000000000ULL / s->period) : 0;
        u64 duty_pct = s->period > 0 ? (s->duty_cycle * 100ULL / s->period) : 0;

        len += sysfs_emit_at(buf, len,
                             "[Channel %d]: Enabled=%s | Freq=%llu Hz | Period=%llu ns | Duty=%llu ns (%llu%%) | Puts=%llu\n",
                             i,
                             s->enabled ? "YES" : "NO",
                             freq_hz,
                             s->period,
                             s->duty_cycle,
                             duty_pct,
                             priv->channels[i].apply_count);
    }
    mutex_unlock(&priv->lock);

    return len;
}
static DEVICE_ATTR_RO(pwm_stats);

static struct platform_device *g_pwm_pdev;

static int mock_pwm_probe(struct platform_device *pdev)
{
    struct pwm_chip *chip;
    struct mock_pwm_priv *priv;
    int ret, i;

    pr_info("Probing Chapter 28 PWM Subsystem Controller\n");

    priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
    if (!priv)
        return -ENOMEM;

    mutex_init(&priv->lock);

    /* Initialize default states for 2 PWM channels */
    for (i = 0; i < NUM_PWMS; ++i) {
        priv->channels[i].state.period     = 1000000; /* 1 ms = 1 kHz */
        priv->channels[i].state.duty_cycle = 500000;  /* 50% duty cycle */
        priv->channels[i].state.polarity   = PWM_POLARITY_NORMAL;
        priv->channels[i].state.enabled    = false;
        priv->channels[i].apply_count      = 0;
    }

    chip = devm_pwmchip_alloc(&pdev->dev, NUM_PWMS, sizeof(void *));
    if (IS_ERR(chip))
        return PTR_ERR(chip);

    chip->ops = &mock_pwm_ops;
    pwmchip_set_drvdata(chip, priv);

    ret = pwmchip_add(chip);
    if (ret) {
        pr_err("pwmchip_add failed (%d)\n", ret);
        return ret;
    }

    ret = device_create_file(&pdev->dev, &dev_attr_pwm_stats);
    if (ret)
        pr_warn("device_create_file failed (%d)\n", ret);

    platform_set_drvdata(pdev, chip);
    g_pwm_priv = priv;

    pr_info("PWM Controller initialized successfully with %d channels!\n", NUM_PWMS);
    return 0;
}

static void mock_pwm_remove(struct platform_device *pdev)
{
    struct pwm_chip *chip = platform_get_drvdata(pdev);

    pr_info("Removing Chapter 28 PWM Controller\n");

    if (chip) {
        device_remove_file(&pdev->dev, &dev_attr_pwm_stats);
        pwmchip_remove(chip);
        g_pwm_priv = NULL;
    }
}

static struct platform_driver mock_pwm_driver = {
    .driver = {
        .name = DRIVER_NAME,
        .owner = THIS_MODULE,
    },
    .probe  = mock_pwm_probe,
    .remove = mock_pwm_remove,
};

static int __init mock_pwm_init(void)
{
    int ret;

    pr_info("Initializing Chapter 28 PWM Subsystem Module\n");

    ret = platform_driver_register(&mock_pwm_driver);
    if (ret)
        return ret;

    g_pwm_pdev = platform_device_register_simple(DRIVER_NAME, -1, NULL, 0);
    if (IS_ERR(g_pwm_pdev)) {
        ret = PTR_ERR(g_pwm_pdev);
        platform_driver_unregister(&mock_pwm_driver);
        return ret;
    }

    return 0;
}

static void __exit mock_pwm_exit(void)
{
    pr_info("Unloading Chapter 28 PWM Subsystem Module\n");

    if (g_pwm_pdev)
        platform_device_unregister(g_pwm_pdev);

    platform_driver_unregister(&mock_pwm_driver);
    pr_info("PWM module unloaded cleanly\n");
}

module_init(mock_pwm_init);
module_exit(mock_pwm_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Deepak Pandey");
MODULE_DESCRIPTION("Chapter 28: Linux PWM (Pulse Width Modulation) Subsystem Controller");
MODULE_VERSION("1.0");
