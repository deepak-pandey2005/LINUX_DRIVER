// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file ioctl_test.c
 * @brief User-Space Test Client for IOCTL Device Driver (/dev/ioctl_dev).
 * @author Deepak Pandey
 *
 * Exercises all 4 IOCTL operations + fault injection and boundary testing:
 *  1. Initial status query (IOCTL_GET_STATUS)
 *  2. Hardware configuration update (IOCTL_SET_CONFIG)
 *  3. In-place data transformation & checksum verification (IOCTL_TRANSFORM_DATA)
 *  4. Device reset (IOCTL_RESET_DEVICE)
 *  5. Invalid magic/command rejection (-ENOTTY)
 *  6. Invalid parameter validation (-EINVAL)
 *  7. Fault injection with invalid user pointers (-EFAULT)
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>

#include "ioctl_commands.h"

#define DEVICE_PATH "/dev/ioctl_dev"

/* ANSI Colors */
#define COLOR_RESET   "\033[0m"
#define COLOR_GREEN   "\033[32;1m"
#define COLOR_RED     "\033[31;1m"
#define COLOR_CYAN    "\033[36;1m"
#define COLOR_YELLOW  "\033[33;1m"

static int g_pass = 0;
static int g_fail = 0;

static void report_result(const char *test_name, int condition, const char *details)
{
    if (condition) {
        printf("  [%sPASS%s] %-42s %s\n", COLOR_GREEN, COLOR_RESET, test_name, details ? details : "");
        g_pass++;
    } else {
        printf("  [%sFAIL%s] %-42s %s\n", COLOR_RED, COLOR_RESET, test_name, details ? details : "");
        g_fail++;
    }
}

int main(void)
{
    int fd;
    int ret;

    printf("\n%s===================================================================%s\n", COLOR_CYAN, COLOR_RESET);
    printf("%s   Chapter 08: IOCTL Subsystem & Custom Commands Test Suite        %s\n", COLOR_CYAN, COLOR_RESET);
    printf("%s===================================================================%s\n\n", COLOR_CYAN, COLOR_RESET);

    fd = open(DEVICE_PATH, O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "%sError opening %s: %s (errno %d)%s\n",
                COLOR_RED, DEVICE_PATH, strerror(errno), errno, COLOR_RESET);
        fprintf(stderr, "%sDid you load the module? Run: sudo insmod ioctl_module.ko%s\n\n",
                COLOR_YELLOW, COLOR_RESET);
        return EXIT_FAILURE;
    }
    printf("[+] Successfully opened %s (fd=%d)\n\n", DEVICE_PATH, fd);

    /* Test 1: Query initial device status */
    {
        struct device_status st;
        memset(&st, 0, sizeof(st));

        ret = ioctl(fd, IOCTL_GET_STATUS, &st);
        char details[128];
        snprintf(details, sizeof(details), "(ops=%u, uptime=%us, temp=%.2f C, ready=%u)",
                 st.total_operations, st.uptime_seconds,
                 (double)st.temperature_mC / 1000.0, st.device_ready);
        report_result("Test 1: Query Initial Status (IOCTL_GET_STATUS)", ret == 0 && st.device_ready == 1, details);
    }

    /* Test 2: Apply hardware configuration */
    {
        struct device_config cfg = {
            .baud_rate = 921600,
            .timeout_ms = 2500,
            .operation_mode = 1 // Turbo mode
        };

        ret = ioctl(fd, IOCTL_SET_CONFIG, &cfg);
        char details[128];
        snprintf(details, sizeof(details), "(set baud=%u, timeout=%ums, mode=%u)",
                 cfg.baud_rate, cfg.timeout_ms, cfg.operation_mode);
        report_result("Test 2: Set Configuration (IOCTL_SET_CONFIG)", ret == 0, details);
    }

    /* Test 3: Verify operational status updated after config change */
    {
        struct device_status st;
        ret = ioctl(fd, IOCTL_GET_STATUS, &st);
        char details[128];
        snprintf(details, sizeof(details), "(total_operations=%u)", st.total_operations);
        report_result("Test 3: Verify Operations Counter Incremented", ret == 0 && st.total_operations >= 1, details);
    }

    /* Test 4: Bidirectional data transformation & checksum */
    {
        struct exchange_payload payload;
        memset(&payload, 0, sizeof(payload));
        strncpy(payload.message, "linux kernel ioctl subsystem rocks!", sizeof(payload.message) - 1);

        printf("      Input string:  \"%s\"\n", payload.message);
        ret = ioctl(fd, IOCTL_TRANSFORM_DATA, &payload);
        printf("      Output string: \"%s\" (checksum=0x%08X)\n", payload.message, payload.checksum);

        int is_upper = (strcmp(payload.message, "LINUX KERNEL IOCTL SUBSYSTEM ROCKS!") == 0);
        int has_chk = (payload.checksum != 0);

        report_result("Test 4: Bidirectional Data (IOCTL_TRANSFORM_DATA)",
                      ret == 0 && is_upper && has_chk,
                      "(uppercase verified, checksum non-zero)");
    }

    /* Test 5: Reset device */
    {
        ret = ioctl(fd, IOCTL_RESET_DEVICE);
        report_result("Test 5: Reset Device (IOCTL_RESET_DEVICE)", ret == 0, "(state & stats cleared)");

        /* Verify total_operations reset back to 0 */
        struct device_status st;
        ioctl(fd, IOCTL_GET_STATUS, &st);
        report_result("Test 5b: Verify Reset Counters", st.total_operations == 0, "(ops=0 confirmed)");
    }

    /* Test 6: Invalid IOCTL Command Rejection (-ENOTTY) */
    {
        #define INVALID_IOCTL_CMD _IO('z', 99)
        errno = 0;
        ret = ioctl(fd, INVALID_IOCTL_CMD);
        char details[128];
        snprintf(details, sizeof(details), "(expected ENOTTY=25, got errno=%d: %s)",
                 errno, strerror(errno));
        report_result("Test 6: Invalid Command Rejection", ret == -1 && errno == ENOTTY, details);
    }

    /* Test 7: Parameter Validation Rejection (-EINVAL) */
    {
        struct device_config invalid_cfg = {
            .baud_rate = 50, // Invalid: below minimum 1200
            .timeout_ms = 1000,
            .operation_mode = 99 // Invalid: modes only 0, 1, 2
        };

        errno = 0;
        ret = ioctl(fd, IOCTL_SET_CONFIG, &invalid_cfg);
        char details[128];
        snprintf(details, sizeof(details), "(expected EINVAL=22, got errno=%d: %s)",
                 errno, strerror(errno));
        report_result("Test 7: Parameter Boundary Rejection", ret == -1 && errno == EINVAL, details);
    }

    /* Test 8: Memory Fault Injection with Invalid User Pointer (-EFAULT) */
    {
        errno = 0;
        ret = ioctl(fd, IOCTL_GET_STATUS, (void *)0x1234);
        char details[128];
        snprintf(details, sizeof(details), "(expected EFAULT=14, got errno=%d: %s)",
                 errno, strerror(errno));
        report_result("Test 8: Invalid Pointer Rejection", ret == -1 && errno == EFAULT, details);
    }

    close(fd);

    /* Final Summary */
    printf("\n%s===================================================================%s\n", COLOR_CYAN, COLOR_RESET);
    printf("   Test Results: %s%d Passed%s, %s%d Failed%s (Total: %d)\n",
           COLOR_GREEN, g_pass, COLOR_RESET,
           g_fail > 0 ? COLOR_RED : COLOR_GREEN, g_fail, COLOR_RESET,
           g_pass + g_fail);
    printf("%s===================================================================%s\n\n", COLOR_CYAN, COLOR_RESET);

    return (g_fail == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
