// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file test_client.c
 * @brief Chapter 19: User-space verification suite for I2C Sensor Subsystem
 * @author Deepak Pandey
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>

#include "i2c_sensor_ioctl.h"

#define DEV_PATH "/dev/i2c_temp_sensor"

#define COLOR_RESET  "\033[0m"
#define COLOR_GREEN  "\033[32m"
#define COLOR_RED    "\033[31m"
#define COLOR_BLUE   "\033[34m"
#define COLOR_YELLOW "\033[33m"

int main(void)
{
    int fd;
    struct i2c_temp_data data;
    char text_buf[512];
    ssize_t bytes;
    int pass_count = 0;
    int test_count = 0;

    printf("%s=================================================================%s\n", COLOR_BLUE, COLOR_RESET);
    printf("%s       Linux I2C Bus & Sensor Subsystem Verification Suite       %s\n", COLOR_BLUE, COLOR_RESET);
    printf("%s=================================================================%s\n\n", COLOR_BLUE, COLOR_RESET);

    fd = open(DEV_PATH, O_RDWR);
    if (fd < 0) {
        printf("%s[FAIL]%s Unable to open %s: %s\n", COLOR_RED, COLOR_RESET, DEV_PATH, strerror(errno));
        printf("Did you run 'make load'?\n");
        return 1;
    }

    /* Test 1: Query Sensor Temperature & Chip ID via IOCTL */
    test_count++;
    printf("[%sTEST %d: QUERYING I2C SENSOR OVER SMBUS VIA IOCTL%s]\n", COLOR_YELLOW, test_count, COLOR_RESET);
    if (ioctl(fd, I2C_IOCTL_GET_TEMP, &data) == 0) {
        printf(" -> Chip ID Register: 0x%02X (Expected: 0x75)\n", data.chip_id);
        printf(" -> Temperature     : %d.%02d C (%d milli-Celsius)\n",
               data.temp_milli_celsius / 1000,
               abs(data.temp_milli_celsius % 1000) / 10,
               data.temp_milli_celsius);

        if (data.chip_id == 0x75 && data.temp_milli_celsius > 0) {
            printf("%s[PASS]%s Sensor communicated successfully over I2C bus!\n\n", COLOR_GREEN, COLOR_RESET);
            pass_count++;
        } else {
            printf("%s[FAIL]%s Invalid sensor data received.\n\n", COLOR_RED, COLOR_RESET);
        }
    } else {
        perror("I2C_IOCTL_GET_TEMP failed");
    }

    /* Test 2: Modify Configuration Register */
    test_count++;
    printf("[%sTEST %d: WRITING CONFIGURATION REGISTER OVER I2C%s]\n", COLOR_YELLOW, test_count, COLOR_RESET);
    __u8 new_conf = 0x18; /* Shutdown mode bitmask */
    if (ioctl(fd, I2C_IOCTL_SET_CONFIG, &new_conf) == 0) {
        ioctl(fd, I2C_IOCTL_GET_TEMP, &data);
        printf(" -> Verified Config Register: 0x%02X\n", data.config_reg);
        if (data.config_reg == new_conf) {
            printf("%s[PASS]%s I2C SMBus write transaction completed and verified!\n\n", COLOR_GREEN, COLOR_RESET);
            pass_count++;
        } else {
            printf("%s[FAIL]%s Config register mismatch.\n\n", COLOR_RED, COLOR_RESET);
        }
    } else {
        perror("I2C_IOCTL_SET_CONFIG failed");
    }

    /* Test 3: Read Structured Telemetry Output */
    test_count++;
    printf("[%sTEST %d: READING STRUCTURED TELEMETRY DUMP%s]\n", COLOR_YELLOW, test_count, COLOR_RESET);
    bytes = read(fd, text_buf, sizeof(text_buf) - 1);
    if (bytes > 0) {
        text_buf[bytes] = '\0';
        printf("%s\n", text_buf);
        if (strstr(text_buf, "LM75") != NULL && strstr(text_buf, "0x48") != NULL) {
            printf("%s[PASS]%s Telemetry read successfully.\n\n", COLOR_GREEN, COLOR_RESET);
            pass_count++;
        } else {
            printf("%s[FAIL]%s Telemetry format mismatch.\n\n", COLOR_RED, COLOR_RESET);
        }
    } else {
        printf("%s[FAIL]%s Read returned 0 bytes.\n\n", COLOR_RED, COLOR_RESET);
    }

    close(fd);

    printf("%s=================================================================%s\n", COLOR_BLUE, COLOR_RESET);
    printf(" Final Result: %s%d/%d passed%s\n",
           (pass_count == test_count) ? COLOR_GREEN : COLOR_RED,
           pass_count, test_count, COLOR_RESET);
    printf("%s=================================================================%s\n", COLOR_BLUE, COLOR_RESET);

    return (pass_count == test_count) ? 0 : 1;
}
