// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file sensor_math.h
 * @brief Header declaring math and sensor conversion helpers across compilation units.
 * @author Deepak Pandey
 */

#ifndef SENSOR_MATH_H
#define SENSOR_MATH_H

#include <linux/types.h>

/**
 * @brief Converts raw ADC 12-bit reading (0-4095) to millivolts.
 * @param raw_adc Raw 12-bit digital value.
 * @param vref_mv Reference voltage in millivolts (e.g. 3300 for 3.3V).
 * @return Voltage in millivolts.
 */
int adc_to_millivolts(u16 raw_adc, u16 vref_mv);

/**
 * @brief Converts temperature from Celsius to Fahrenheit.
 * @param temp_c Temperature in degrees Celsius.
 * @return Temperature in degrees Fahrenheit.
 */
int celsius_to_fahrenheit(int temp_c);

#endif /* SENSOR_MATH_H */
