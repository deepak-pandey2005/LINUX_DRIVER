// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file test_client.c
 * @brief Chapter 18: User-space verification suite for GPIO Subsystem
 * @author Deepak Pandey
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>

#include "gpio_ioctl.h"

#define DEV_PATH "/dev/virtual_gpio"

#define COLOR_RESET  "\033[0m"
#define COLOR_GREEN  "\033[32m"
#define COLOR_RED    "\033[31m"
#define COLOR_BLUE   "\033[34m"
#define COLOR_YELLOW "\033[33m"

int main(void)
{
    int fd;
    struct gpio_chip_info info;
    struct gpio_pin_req req;
    int pass_count = 0;
    int test_count = 0;

    printf("%s=================================================================%s\n", COLOR_BLUE, COLOR_RESET);
    printf("%s         Linux GPIO Subsystem (gpiod) Verification Suite         %s\n", COLOR_BLUE, COLOR_RESET);
    printf("%s=================================================================%s\n\n", COLOR_BLUE, COLOR_RESET);

    fd = open(DEV_PATH, O_RDWR);
    if (fd < 0) {
        printf("%s[FAIL]%s Unable to open %s: %s\n", COLOR_RED, COLOR_RESET, DEV_PATH, strerror(errno));
        printf("Did you run 'make load'?\n");
        return 1;
    }

    /* Test 1: Query GPIO Chip Information */
    test_count++;
    printf("[%sTEST %d: GPIO CONTROLLER DISCOVERY%s]\n", COLOR_YELLOW, test_count, COLOR_RESET);
    if (ioctl(fd, GPIO_IOCTL_GET_CHIP_INFO, &info) == 0) {
        printf(" -> GPIO Base Number: %d\n", info.base);
        printf(" -> Number of GPIOs : %u lines\n", info.ngpio);
        printf(" -> Pin Values Mask : 0x%02X\n", info.pin_values);
        printf(" -> Direction Mask  : 0x%02X (1=IN, 0=OUT)\n", info.pin_directions);
        if (info.ngpio == 8) {
            printf("%s[PASS]%s Virtual GPIO chip verified!\n\n", COLOR_GREEN, COLOR_RESET);
            pass_count++;
        } else {
            printf("%s[FAIL]%s Expected 8 GPIO lines, got %u\n\n", COLOR_RED, COLOR_RESET, info.ngpio);
        }
    } else {
        perror("GET_CHIP_INFO failed");
    }

    /* Test 2: Configure Pin 0 as OUTPUT and Write HIGH (1) */
    test_count++;
    printf("[%sTEST %d: CONFIGURE PIN 0 AS OUTPUT AND SET HIGH (1)%s]\n", COLOR_YELLOW, test_count, COLOR_RESET);
    req.pin_offset = 0;
    req.value = 1;
    if (ioctl(fd, GPIO_IOCTL_SET_DIR_OUT, &req) == 0) {
        req.value = 99;
        ioctl(fd, GPIO_IOCTL_READ_PIN, &req);
        printf(" -> Pin 0 readback value: %u\n", req.value);
        if (req.value == 1) {
            printf("%s[PASS]%s Pin 0 set to OUTPUT HIGH and verified.\n\n", COLOR_GREEN, COLOR_RESET);
            pass_count++;
        } else {
            printf("%s[FAIL]%s Expected value 1, read %u\n\n", COLOR_RED, COLOR_RESET, req.value);
        }
    } else {
        perror("SET_DIR_OUT failed");
    }

    /* Test 3: Set Pin 0 to LOW (0) */
    test_count++;
    printf("[%sTEST %d: WRITE LOW (0) TO PIN 0%s]\n", COLOR_YELLOW, test_count, COLOR_RESET);
    req.pin_offset = 0;
    req.value = 0;
    if (ioctl(fd, GPIO_IOCTL_WRITE_PIN, &req) == 0) {
        ioctl(fd, GPIO_IOCTL_READ_PIN, &req);
        printf(" -> Pin 0 readback value: %u\n", req.value);
        if (req.value == 0) {
            printf("%s[PASS]%s Pin 0 set to LOW and verified.\n\n", COLOR_GREEN, COLOR_RESET);
            pass_count++;
        } else {
            printf("%s[FAIL]%s Expected value 0, read %u\n\n", COLOR_RED, COLOR_RESET, req.value);
        }
    } else {
        perror("WRITE_PIN failed");
    }

    /* Test 4: Walk all 8 pins and write alternating pattern (0xAA = 10101010b) */
    test_count++;
    printf("[%sTEST %d: WALKING PATTERN ACROSS ALL 8 GPIO LINES (0xAA)%s]\n", COLOR_YELLOW, test_count, COLOR_RESET);
    for (int i = 0; i < 8; i++) {
        req.pin_offset = i;
        req.value = (i % 2 == 1) ? 1 : 0;
        ioctl(fd, GPIO_IOCTL_SET_DIR_OUT, &req);
    }
    ioctl(fd, GPIO_IOCTL_GET_CHIP_INFO, &info);
    printf(" -> Full controller bitmask: 0x%02X (Expected: 0xAA)\n", info.pin_values & 0xFF);
    if ((info.pin_values & 0xFF) == 0xAA) {
        printf("%s[PASS]%s All 8 pins independently manipulated with modern gpiod APIs!\n\n",
               COLOR_GREEN, COLOR_RESET);
        pass_count++;
    } else {
        printf("%s[FAIL]%s Pattern mismatch.\n\n", COLOR_RED, COLOR_RESET);
    }

    close(fd);

    printf("%s=================================================================%s\n", COLOR_BLUE, COLOR_RESET);
    printf(" Final Result: %s%d/%d passed%s\n",
           (pass_count == test_count) ? COLOR_GREEN : COLOR_RED,
           pass_count, test_count, COLOR_RESET);
    printf("%s=================================================================%s\n", COLOR_BLUE, COLOR_RESET);

    return (pass_count == test_count) ? 0 : 1;
}
