// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file sensor_math.c
 * @brief Secondary compilation unit providing mathematical conversion routines.
 * @author Deepak Pandey
 *
 * Notice this file does NOT contain module_init or module_exit.
 * It is linked alongside multi_main.c into a single .ko module.
 */

#include "sensor_math.h"
#include <linux/kernel.h>

int adc_to_millivolts(u16 raw_adc, u16 vref_mv)
{
    // Clamp to 12-bit max (4095)
    if (raw_adc > 4095) {
        raw_adc = 4095;
    }

    // Fixed-point calculation avoiding float: (raw * vref) / 4095
    return (int)(((u32)raw_adc * (u32)vref_mv) / 4095);
}

int celsius_to_fahrenheit(int temp_c)
{
    // Formula: (C * 9 / 5) + 32
    return (temp_c * 9 / 5) + 32;
}
