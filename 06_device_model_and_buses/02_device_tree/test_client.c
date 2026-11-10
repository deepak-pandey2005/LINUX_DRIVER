// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file test_client.c
 * @brief Chapter 17: User-space verification suite for Device Tree Property Parsing
 * @author Deepak Pandey
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#define DEV_PATH "/dev/dt_sensor"

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
    printf("%s    Linux Device Tree & OF Property Parsing Verification Suite   %s\n", COLOR_BLUE, COLOR_RESET);
    printf("%s=================================================================%s\n\n", COLOR_BLUE, COLOR_RESET);

    test_count++;
    printf("[%sTEST %d: OPENING /dev/dt_sensor%s]\n", COLOR_YELLOW, test_count, COLOR_RESET);
    fd = open(DEV_PATH, O_RDONLY);
    if (fd < 0) {
        printf("%s[FAIL]%s Unable to open %s: %s\n", COLOR_RED, COLOR_RESET, DEV_PATH, strerror(errno));
        printf("Did you run 'make load'?\n");
        return 1;
    }
    printf("%s[PASS]%s Character device opened successfully.\n\n", COLOR_GREEN, COLOR_RESET);
    pass_count++;

    test_count++;
    printf("[%sTEST %d: PARSING AND VERIFYING DT PROPERTIES%s]\n", COLOR_YELLOW, test_count, COLOR_RESET);
    bytes = read(fd, buf, sizeof(buf) - 1);
    close(fd);

    if (bytes > 0) {
        buf[bytes] = '\0';
        printf(" -> Kernel telemetry report:\n%s\n", buf);

        int has_clock = (strstr(buf, "400000 Hz") != NULL);
        int has_rate  = (strstr(buf, "100 Hz") != NULL);
        int has_label = (strstr(buf, "ACCEL_GYRO_V2") != NULL);
        int has_power = (strstr(buf, "ENABLED") != NULL);

        if (has_clock && has_rate && has_label && has_power) {
            printf("%s[PASS]%s All Device Tree properties parsed with 100%% accuracy!\n\n",
                   COLOR_GREEN, COLOR_RESET);
            pass_count++;
        } else {
            printf("%s[FAIL]%s Property mismatch detected in kernel telemetry.\n\n", COLOR_RED, COLOR_RESET);
        }
    } else {
        printf("%s[FAIL]%s Read failed or returned 0 bytes.\n\n", COLOR_RED, COLOR_RESET);
    }

    printf("%s=================================================================%s\n", COLOR_BLUE, COLOR_RESET);
    printf(" Final Result: %s%d/%d passed%s\n",
           (pass_count == test_count) ? COLOR_GREEN : COLOR_RED,
           pass_count, test_count, COLOR_RESET);
    printf("%s=================================================================%s\n", COLOR_BLUE, COLOR_RESET);

    return (pass_count == test_count) ? 0 : 1;
}

