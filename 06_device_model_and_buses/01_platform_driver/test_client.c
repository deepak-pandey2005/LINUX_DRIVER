// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file test_client.c
 * @brief Chapter 16: User-space verification suite for Platform Driver & Device
 * @author Deepak Pandey
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#define DEV_PATH "/dev/platform_demo"
#define SYSFS_DEV_PATH "/sys/bus/platform/devices/demo_platform_dev.0"
#define SYSFS_DRV_PATH "/sys/bus/platform/drivers/demo_platform_drv"

#define COLOR_RESET  "\033[0m"
#define COLOR_GREEN  "\033[32m"
#define COLOR_RED    "\033[31m"
#define COLOR_BLUE   "\033[34m"
#define COLOR_YELLOW "\033[33m"

int main(void)
{
    int fd;
    char buf[512];
    ssize_t bytes;
    int pass_count = 0;
    int test_count = 0;

    printf("%s=================================================================%s\n", COLOR_BLUE, COLOR_RESET);
    printf("%s     Linux Platform Driver & Device Model Verification Suite     %s\n", COLOR_BLUE, COLOR_RESET);
    printf("%s=================================================================%s\n\n", COLOR_BLUE, COLOR_RESET);

    /* Test 1: Verify platform device presence in sysfs */
    test_count++;
    printf("[%sTEST %d: PLATFORM BUS SYSFS DEVICE NODE%s]\n", COLOR_YELLOW, test_count, COLOR_RESET);
    if (access(SYSFS_DEV_PATH, F_OK) == 0) {
        printf(" -> Found sysfs platform node: %s\n", SYSFS_DEV_PATH);
        printf("%s[PASS]%s Platform device successfully registered on platform bus!\n\n", COLOR_GREEN, COLOR_RESET);
        pass_count++;
    } else {
        printf("%s[FAIL]%s Cannot find %s: %s\n\n", COLOR_RED, COLOR_RESET, SYSFS_DEV_PATH, strerror(errno));
    }

    /* Test 2: Verify driver registration on platform bus */
    test_count++;
    printf("[%sTEST %d: PLATFORM BUS DRIVER REGISTRATION%s]\n", COLOR_YELLOW, test_count, COLOR_RESET);
    if (access(SYSFS_DRV_PATH, F_OK) == 0) {
        printf(" -> Found sysfs driver node: %s\n", SYSFS_DRV_PATH);
        printf("%s[PASS]%s Platform driver registered and active.\n\n", COLOR_GREEN, COLOR_RESET);
        pass_count++;
    } else {
        printf("%s[FAIL]%s Cannot find %s: %s\n\n", COLOR_RED, COLOR_RESET, SYSFS_DRV_PATH, strerror(errno));
    }

    /* Test 3: Read character device created by probe() */
    test_count++;
    printf("[%sTEST %d: READING PROBED DEVICE TELEMETRY%s]\n", COLOR_YELLOW, test_count, COLOR_RESET);
    fd = open(DEV_PATH, O_RDONLY);
    if (fd < 0) {
        printf("%s[FAIL]%s Cannot open %s: %s\n\n", COLOR_RED, COLOR_RESET, DEV_PATH, strerror(errno));
    } else {
        bytes = read(fd, buf, sizeof(buf) - 1);
        close(fd);
        if (bytes > 0) {
            buf[bytes] = '\0';
            printf(" -> Telemetry read from /dev/platform_demo:\n%s\n", buf);
            if (strstr(buf, "Base Memory") != NULL && strstr(buf, "demo_platform_drv") != NULL) {
                printf("%s[PASS]%s Probe callback properly extracted MEM & IRQ resources!\n\n",
                       COLOR_GREEN, COLOR_RESET);
                pass_count++;
            } else {
                printf("%s[FAIL]%s Incomplete resource content.\n\n", COLOR_RED, COLOR_RESET);
            }
        } else {
            printf("%s[FAIL]%s Read returned 0 bytes.\n\n", COLOR_RED, COLOR_RESET);
        }
    }

    printf("%s=================================================================%s\n", COLOR_BLUE, COLOR_RESET);
    printf(" Final Result: %s%d/%d passed%s\n",
           (pass_count == test_count) ? COLOR_GREEN : COLOR_RED,
           pass_count, test_count, COLOR_RESET);
    printf("%s=================================================================%s\n", COLOR_BLUE, COLOR_RESET);

    return (pass_count == test_count) ? 0 : 1;
}

