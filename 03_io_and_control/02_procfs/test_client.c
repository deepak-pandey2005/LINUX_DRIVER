// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file test_client.c
 * @brief User-Space Diagnostic & Testing Client for /proc/driver_diagnostics.
 * @author Deepak Pandey
 *
 * Validates:
 *  1. Read-only access to /proc/driver_diagnostics/status via seq_file.
 *  2. Parsing formatted diagnostic telemetry.
 *  3. Reading /proc/driver_diagnostics/config.
 *  4. Dynamically reconfiguring runtime parameters via procfs writes.
 *  5. Verifying diagnostic counter increments.
 *  6. Defensive validation: rejecting invalid configuration inputs (-EINVAL).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#define PROC_STATUS_PATH "/proc/driver_diagnostics/status"
#define PROC_CONFIG_PATH "/proc/driver_diagnostics/config"

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
        printf("  [%sPASS%s] %-44s %s\n", COLOR_GREEN, COLOR_RESET, test_name, details ? details : "");
        g_pass++;
    } else {
        printf("  [%sFAIL%s] %-44s %s\n", COLOR_RED, COLOR_RESET, test_name, details ? details : "");
        g_fail++;
    }
}

int main(void)
{
    int fd;
    ssize_t bytes;
    char buffer[1024];

    printf("\n%s===================================================================%s\n", COLOR_CYAN, COLOR_RESET);
    printf("%s   Chapter 09: Procfs Diagnostic Subsystem Test Suite              %s\n", COLOR_CYAN, COLOR_RESET);
    printf("%s===================================================================%s\n\n", COLOR_CYAN, COLOR_RESET);

    /* Test 1: Read status entry */
    {
        fd = open(PROC_STATUS_PATH, O_RDONLY);
        if (fd < 0) {
            fprintf(stderr, "%sError opening %s: %s (errno %d)%s\n",
                    COLOR_RED, PROC_STATUS_PATH, strerror(errno), errno, COLOR_RESET);
            fprintf(stderr, "%sDid you load the module? Run: sudo insmod procfs_module.ko%s\n\n",
                    COLOR_YELLOW, COLOR_RESET);
            return EXIT_FAILURE;
        }

        memset(buffer, 0, sizeof(buffer));
        bytes = read(fd, buffer, sizeof(buffer) - 1);
        close(fd);

        int has_active = (strstr(buffer, "Driver Status        : ACTIVE") != NULL);
        int has_module = (strstr(buffer, "Kernel Module        : procfs_module") != NULL);

        report_result("Test 1: Read /proc Status (seq_file)", bytes > 0 && has_active && has_module,
                      "(header & active status verified)");

        printf("\n--- Diagnostic Snapshot ---\n%s---------------------------\n\n", buffer);
    }

    /* Test 2: Read current config */
    {
        fd = open(PROC_CONFIG_PATH, O_RDONLY);
        memset(buffer, 0, sizeof(buffer));
        bytes = read(fd, buffer, sizeof(buffer) - 1);
        close(fd);

        int has_debug = (strstr(buffer, "debug_level=") != NULL);
        int has_profile = (strstr(buffer, "profile=") != NULL);

        report_result("Test 2: Read /proc Config Entry", bytes > 0 && has_debug && has_profile,
                      "(keys debug_level & profile parsed)");
    }

    /* Test 3: Write new debug_level */
    {
        const char *new_debug = "debug_level=2\n";
        fd = open(PROC_CONFIG_PATH, O_WRONLY);
        bytes = write(fd, new_debug, strlen(new_debug));
        close(fd);

        report_result("Test 3: Update Config (debug_level=2)", bytes == (ssize_t)strlen(new_debug),
                      "(wrote debug_level=2 successfully)");
    }

    /* Test 4: Write new hardware profile */
    {
        const char *new_profile = "profile=turbo_low_latency\n";
        fd = open(PROC_CONFIG_PATH, O_WRONLY);
        bytes = write(fd, new_profile, strlen(new_profile));
        close(fd);

        report_result("Test 4: Update Config (profile=turbo_low_latency)", bytes == (ssize_t)strlen(new_profile),
                      "(wrote new profile name)");
    }

    /* Test 5: Verify updated values in config */
    {
        fd = open(PROC_CONFIG_PATH, O_RDONLY);
        memset(buffer, 0, sizeof(buffer));
        bytes = read(fd, buffer, sizeof(buffer) - 1);
        close(fd);

        int matches_debug = (strstr(buffer, "debug_level=2") != NULL);
        int matches_profile = (strstr(buffer, "profile=turbo_low_latency") != NULL);

        report_result("Test 5: Verify Updated Configuration State",
                      bytes > 0 && matches_debug && matches_profile,
                      "(persisted values matched)");
    }

    /* Test 6: Verify write count incremented in status */
    {
        fd = open(PROC_STATUS_PATH, O_RDONLY);
        memset(buffer, 0, sizeof(buffer));
        bytes = read(fd, buffer, sizeof(buffer) - 1);
        close(fd);

        int has_writes = (strstr(buffer, "Config Writes        : 2") != NULL);
        report_result("Test 6: Verify Status Telemetry Synchronization",
                      bytes > 0 && has_writes,
                      "(config writes counter = 2)");
    }

    /* Test 7: Reject invalid config syntax (-EINVAL) */
    {
        const char *invalid_syntax = "invalid_format_without_equals\n";
        fd = open(PROC_CONFIG_PATH, O_WRONLY);
        errno = 0;
        bytes = write(fd, invalid_syntax, strlen(invalid_syntax));
        close(fd);

        char details[128];
        snprintf(details, sizeof(details), "(expected EINVAL=22, got errno=%d: %s)",
                 errno, strerror(errno));
        report_result("Test 7: Reject Malformed Syntax Rejection",
                      bytes == -1 && errno == EINVAL, details);
    }

    /* Test 8: Reject out-of-bounds parameter (-EINVAL) */
    {
        const char *invalid_val = "debug_level=99\n";
        fd = open(PROC_CONFIG_PATH, O_WRONLY);
        errno = 0;
        bytes = write(fd, invalid_val, strlen(invalid_val));
        close(fd);

        char details[128];
        snprintf(details, sizeof(details), "(expected EINVAL=22, got errno=%d: %s)",
                 errno, strerror(errno));
        report_result("Test 8: Reject Out-Of-Bounds Parameter",
                      bytes == -1 && errno == EINVAL, details);
    }

    /* Final Summary */
    printf("\n%s===================================================================%s\n", COLOR_CYAN, COLOR_RESET);
    printf("   Test Results: %s%d Passed%s, %s%d Failed%s (Total: %d)\n",
           COLOR_GREEN, g_pass, COLOR_RESET,
           g_fail > 0 ? COLOR_RED : COLOR_GREEN, g_fail, COLOR_RESET,
           g_pass + g_fail);
    printf("%s===================================================================%s\n\n", COLOR_CYAN, COLOR_RESET);

    return (g_fail == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
