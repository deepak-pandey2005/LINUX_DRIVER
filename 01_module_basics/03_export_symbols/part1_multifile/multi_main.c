// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file multi_main.c
 * @brief Primary entry point for multi-file kernel module.
 * @author Deepak Pandey
 *
 * Demonstrates calling functions defined across separate compilation units
 * linked together by Kbuild into a single kernel object.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include "sensor_math.h"

static int __init multi_main_init(void)
{
    u16 raw_sample = 2048; // Half-scale ADC reading (~1.65V on 3.3V ref)
    int voltage_mv;
    int temp_c = 25;
    int temp_f;

    pr_info("====================================================\n");
    pr_info("Multi-file module (multi_file_driver) loaded!\n");
    pr_info("Calling functions defined in sensor_math.c:\n");

    voltage_mv = adc_to_millivolts(raw_sample, 3300);
    temp_f = celsius_to_fahrenheit(temp_c);

    pr_info("  - Raw ADC %u -> Voltage: %d mV\n", raw_sample, voltage_mv);
    pr_info("  - Temperature: %d C -> %d F\n", temp_c, temp_f);
    pr_info("====================================================\n");

    return 0;
}

static void __exit multi_main_exit(void)
{
    pr_info("Multi-file module unloaded cleanly.\n");
}

module_init(multi_main_init);
module_exit(multi_main_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Deepak Pandey");
MODULE_DESCRIPTION("Chapter 03 (Part 1): Multi-File Compilation Unit Demonstration");
MODULE_VERSION("1.0.0");
