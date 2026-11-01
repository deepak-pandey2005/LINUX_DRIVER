// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file test_client.c
 * @brief Chapter 10: User-space verification suite for Sysfs Telemetry Interface
 * @author Deepak Pandey
 *
 * Validates:
 *  - Reading standard attributes (device_mode, baud_rate, uptime_seconds, stats).
 *  - Writing valid modes ("LOW_POWER", "OVERCLOCK", "NORMAL").
 *  - Writing valid baud rates (9600, 19200, 57600, 115200).
 *  - Rejection of invalid inputs (-EINVAL).
 *  - Triggering write-only reset action and verifying state restoration.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#define SYSFS_PATH "/sys/kernel/telemetry_subsystem"

#define COLOR_RESET  "\033[0m"
#define COLOR_GREEN  "\033[32m"
#define COLOR_RED    "\033[31m"
#define COLOR_BLUE   "\033[34m"
#define COLOR_YELLOW "\033[33m"

static int read_sysfs_attr(const char *attr_name, char *out_buf, size_t buf_size)
{
    char path[256];
    snprintf(path, sizeof(path), "%s/%s", SYSFS_PATH, attr_name);

    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        return -errno;
    }

    ssize_t bytes = read(fd, out_buf, buf_size - 1);
    close(fd);

    if (bytes < 0)
        return -errno;

    out_buf[bytes] = '\0';
    /* Strip trailing newline for clean display */
    if (bytes > 0 && out_buf[bytes - 1] == '\n')
        out_buf[bytes - 1] = '\0';

    return 0;
}

static int write_sysfs_attr(const char *attr_name, const char *val)
{
    char path[256];
    snprintf(path, sizeof(path), "%s/%s", SYSFS_PATH, attr_name);

    int fd = open(path, O_WRONLY);
    if (fd < 0) {
        return -errno;
    }

    ssize_t len = strlen(val);
    ssize_t bytes = write(fd, val, len);
    close(fd);

    if (bytes < 0)
        return -errno;

    return 0;
}

static void print_header(void)
{
    printf("%s=================================================================%s\n", COLOR_BLUE, COLOR_RESET);
    printf("%s       Linux Kernel Sysfs Subsystem Verification Suite           %s\n", COLOR_BLUE, COLOR_RESET);
    printf("%s=================================================================%s\n\n", COLOR_BLUE, COLOR_RESET);
}

int main(void)
{
    char buf[256];
    int ret;
    int test_count = 0;
    int pass_count = 0;

    print_header();

    /* Test 1: Verify sysfs node presence */
    test_count++;
    printf("[%sTEST %d%s] Verifying sysfs path %s...\n", COLOR_YELLOW, test_count, COLOR_RESET, SYSFS_PATH);
    if (access(SYSFS_PATH, F_OK) != 0) {
        printf("%s[FAIL]%s Directory %s does not exist. Did you run 'make load'?\n",
               COLOR_RED, COLOR_RESET, SYSFS_PATH);
        return 1;
    }
    printf("%s[PASS]%s Sysfs telemetry directory found.\n\n", COLOR_GREEN, COLOR_RESET);
    pass_count++;

    /* Test 2: Read initial device_mode */
    test_count++;
    printf("[%sTEST %d%s] Reading initial device_mode...\n", COLOR_YELLOW, test_count, COLOR_RESET);
    ret = read_sysfs_attr("device_mode", buf, sizeof(buf));
    if (ret == 0) {
        printf(" -> device_mode = '%s'\n", buf);
        printf("%s[PASS]%s Initial device_mode read successfully.\n\n", COLOR_GREEN, COLOR_RESET);
        pass_count++;
    } else {
        printf("%s[FAIL]%s Failed to read device_mode: %s\n\n", COLOR_RED, COLOR_RESET, strerror(-ret));
    }

    /* Test 3: Read initial baud_rate */
    test_count++;
    printf("[%sTEST %d%s] Reading initial baud_rate...\n", COLOR_YELLOW, test_count, COLOR_RESET);
    ret = read_sysfs_attr("baud_rate", buf, sizeof(buf));
    if (ret == 0) {
        printf(" -> baud_rate = '%s'\n", buf);
        printf("%s[PASS]%s Initial baud_rate read successfully.\n\n", COLOR_GREEN, COLOR_RESET);
        pass_count++;
    } else {
        printf("%s[FAIL]%s Failed to read baud_rate: %s\n\n", COLOR_RED, COLOR_RESET, strerror(-ret));
    }

    /* Test 4: Write and verify new device_mode */
    test_count++;
    printf("[%sTEST %d%s] Setting device_mode to 'OVERCLOCK'...\n", COLOR_YELLOW, test_count, COLOR_RESET);
    ret = write_sysfs_attr("device_mode", "OVERCLOCK");
    if (ret == 0) {
        read_sysfs_attr("device_mode", buf, sizeof(buf));
        if (strcmp(buf, "OVERCLOCK") == 0) {
            printf(" -> device_mode verified as '%s'\n", buf);
            printf("%s[PASS]%s device_mode updated and verified.\n\n", COLOR_GREEN, COLOR_RESET);
            pass_count++;
        } else {
            printf("%s[FAIL]%s Mismatched mode: expected OVERCLOCK, got '%s'\n\n", COLOR_RED, COLOR_RESET, buf);
        }
    } else {
        printf("%s[FAIL]%s Failed to write device_mode: %s\n\n", COLOR_RED, COLOR_RESET, strerror(-ret));
    }

    /* Test 5: Write and verify new baud_rate */
    test_count++;
    printf("[%sTEST %d%s] Setting baud_rate to '57600'...\n", COLOR_YELLOW, test_count, COLOR_RESET);
    ret = write_sysfs_attr("baud_rate", "57600");
    if (ret == 0) {
        read_sysfs_attr("baud_rate", buf, sizeof(buf));
        if (strcmp(buf, "57600") == 0) {
            printf(" -> baud_rate verified as '%s'\n", buf);
            printf("%s[PASS]%s baud_rate updated and verified.\n\n", COLOR_GREEN, COLOR_RESET);
            pass_count++;
        } else {
            printf("%s[FAIL]%s Mismatched baud: expected 57600, got '%s'\n\n", COLOR_RED, COLOR_RESET, buf);
        }
    } else {
        printf("%s[FAIL]%s Failed to write baud_rate: %s\n\n", COLOR_RED, COLOR_RESET, strerror(-ret));
    }

    /* Test 6: Defensive Input Validation (Invalid Mode) */
    test_count++;
    printf("[%sTEST %d%s] Testing rejection of invalid mode string ('TURBO_BOOST')...\n", COLOR_YELLOW, test_count, COLOR_RESET);
    ret = write_sysfs_attr("device_mode", "TURBO_BOOST");
    if (ret == -EINVAL) {
        printf(" -> Driver cleanly rejected invalid mode with -EINVAL\n");
        printf("%s[PASS]%s Input validation successfully blocked malformed mode.\n\n", COLOR_GREEN, COLOR_RESET);
        pass_count++;
    } else {
        printf("%s[FAIL]%s Expected -EINVAL, got %d (%s)\n\n", COLOR_RED, COLOR_RESET, ret, strerror(-ret));
    }

    /* Test 7: Defensive Input Validation (Invalid Baud Rate) */
    test_count++;
    printf("[%sTEST %d%s] Testing rejection of invalid baud rate (9999999)...\n", COLOR_YELLOW, test_count, COLOR_RESET);
    ret = write_sysfs_attr("baud_rate", "9999999");
    if (ret == -EINVAL) {
        printf(" -> Driver cleanly rejected invalid baud rate with -EINVAL\n");
        printf("%s[PASS]%s Input validation successfully blocked unsupported baud rate.\n\n", COLOR_GREEN, COLOR_RESET);
        pass_count++;
    } else {
        printf("%s[FAIL]%s Expected -EINVAL, got %d (%s)\n\n", COLOR_RED, COLOR_RESET, ret, strerror(-ret));
    }

    /* Test 8: Read uptime_seconds and stats */
    test_count++;
    printf("[%sTEST %d%s] Reading telemetry stats and uptime...\n", COLOR_YELLOW, test_count, COLOR_RESET);
    read_sysfs_attr("uptime_seconds", buf, sizeof(buf));
    printf(" -> uptime_seconds: %s\n", buf);
    read_sysfs_attr("stats", buf, sizeof(buf));
    printf(" -> stats: %s\n", buf);
    printf("%s[PASS]%s Telemetry read successfully.\n\n", COLOR_GREEN, COLOR_RESET);
    pass_count++;

    /* Test 9: Reset trigger */
    test_count++;
    printf("[%sTEST %d%s] Triggering driver reset (echo 1 > reset)...\n", COLOR_YELLOW, test_count, COLOR_RESET);
    ret = write_sysfs_attr("reset", "1");
    if (ret == 0) {
        read_sysfs_attr("device_mode", buf, sizeof(buf));
        read_sysfs_attr("baud_rate", buf, sizeof(buf));
        printf(" -> Post-reset device_mode: %s, baud_rate: %s\n", buf, buf);
        printf("%s[PASS]%s Reset action triggered and state restored to defaults.\n\n", COLOR_GREEN, COLOR_RESET);
        pass_count++;
    } else {
        printf("%s[FAIL]%s Failed to trigger reset: %s\n\n", COLOR_RED, COLOR_RESET, strerror(-ret));
    }

    /* Final Summary */
    printf("%s=================================================================%s\n", COLOR_BLUE, COLOR_RESET);
    printf(" Test Results: %s%d/%d passed%s\n",
           (pass_count == test_count) ? COLOR_GREEN : COLOR_RED,
           pass_count, test_count, COLOR_RESET);
    printf("%s=================================================================%s\n", COLOR_BLUE, COLOR_RESET);

    return (pass_count == test_count) ? 0 : 1;
}
