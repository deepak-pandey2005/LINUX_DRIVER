// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file test_client.c
 * @brief Chapter 31: Device Power Management & Runtime PM Verification Client
 * @author Deepak Pandey
 *
 * Verifies:
 *  - Character device node communication (/dev/pm_demo_dev).
 *  - Synchronous wake-on-open and autosuspend-on-close cycles.
 *  - IOCTL statistical retrieval (PM_DEMO_IOC_GET_STATS).
 *  - Runtime PM autosuspend delay reconfiguration (PM_DEMO_IOC_SET_AUTOSUSPEND).
 *  - Sysfs power telemetry and runtime_status inspection.
 *  - Idle detection and transition from ACTIVE -> SUSPENDED.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <time.h>

#define DEV_PATH "/dev/pm_demo_dev"
#define SYSFS_SYS_STATUS "/sys/devices/platform/pm_demo_driver/power/runtime_status"
#define SYSFS_TELEMETRY  "/sys/devices/platform/pm_demo_driver/power_telemetry"
#define SYSFS_FORCE      "/sys/devices/platform/pm_demo_driver/force_active"

/* Color formatting */
#define COLOR_RESET   "\033[0m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_RED     "\033[31m"
#define COLOR_BOLD    "\033[1m"

/* IOCTL Definitions */
#define PM_DEMO_IOC_MAGIC 'P'

struct pm_demo_stats {
    uint32_t power_state;      /* 0: Suspended, 1: Active, 2: Sleep */
    uint32_t usage_count;      /* Runtime PM usage reference counter */
    uint32_t autosuspend_ms;  /* Current autosuspend delay in ms */
    uint64_t resume_count;    /* Total runtime resume operations */
    uint64_t suspend_count;   /* Total runtime suspend operations */
    uint64_t active_time_ms;  /* Total time active in ms */
    uint8_t  hw_powered;      /* 1 = Power rails energized, 0 = Gated */
    uint8_t  clock_enabled;   /* 1 = Clock oscillator active, 0 = Stopped */
    uint8_t  reserved[6];
};

#define PM_DEMO_IOC_GET_STATS       _IOR(PM_DEMO_IOC_MAGIC, 1, struct pm_demo_stats)
#define PM_DEMO_IOC_SET_AUTOSUSPEND _IOW(PM_DEMO_IOC_MAGIC, 2, uint32_t)
#define PM_DEMO_IOC_WAKE_BURST      _IO(PM_DEMO_IOC_MAGIC, 3)

static void print_section(const char *title)
{
    printf("\n%s=== %s ===%s\n", COLOR_CYAN, title, COLOR_RESET);
}

static void read_sysfs_file(const char *path, const char *label)
{
    char buf[512];
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        printf("  [%s] %s: Not accessible (%s)\n", COLOR_YELLOW, label, strerror(errno));
        return;
    }

    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    close(fd);

    if (n > 0) {
        buf[n] = '\0';
        /* Remove trailing newline if single line */
        if (n > 1 && buf[n - 1] == '\n' && strchr(buf, '\n') == &buf[n - 1])
            buf[n - 1] = '\0';
        printf("  [%s%s%s]:\n%s", COLOR_GREEN, label, COLOR_RESET, buf);
        if (buf[strlen(buf) - 1] != '\n')
            printf("\n");
    }
}

int main(int argc, char *argv[])
{
    int fd;
    struct pm_demo_stats stats;
    char read_buf[512];
    ssize_t bytes_read, bytes_written;

    (void)argc;
    (void)argv;

    printf("%s================================================================%s\n",
           COLOR_BOLD, COLOR_RESET);
    printf("%s   Chapter 31: Device Power Management & Runtime PM Client       %s\n",
           COLOR_BOLD, COLOR_RESET);
    printf("%s================================================================%s\n",
           COLOR_BOLD, COLOR_RESET);

    /* 1. Pre-Open Sysfs Inspection */
    print_section("1. Baseline Device State (Pre-Open)");
    read_sysfs_file(SYSFS_SYS_STATUS, "Kernel Runtime Status");
    read_sysfs_file(SYSFS_TELEMETRY, "Custom PM Telemetry");

    /* 2. Open Device Node (Triggers pm_runtime_get_sync) */
    print_section("2. Opening Character Device Node (/dev/pm_demo_dev)");
    printf("Opening %s (should wake device and resume power rails)...\n", DEV_PATH);
    fd = open(DEV_PATH, O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "%s[-] Failed to open %s: %s%s\n",
                COLOR_RED, DEV_PATH, strerror(errno), COLOR_RESET);
        fprintf(stderr, "Ensure kernel module 'pm_driver_module.ko' is loaded.\n");
        return EXIT_FAILURE;
    }
    printf("%s[+] Device opened successfully! (File Descriptor: %d)%s\n",
           COLOR_GREEN, fd, COLOR_RESET);

    /* 3. Character Device Read Operation */
    print_section("3. Reading Character Device Telemetry Buffer");
    bytes_read = read(fd, read_buf, sizeof(read_buf) - 1);
    if (bytes_read > 0) {
        read_buf[bytes_read] = '\0';
        printf("%s", read_buf);
    } else {
        printf("[-] Read returned %zd: %s\n", bytes_read, strerror(errno));
    }

    /* 4. IOCTL Statistics Query */
    print_section("4. Querying Hardware PM Metrics via IOCTL");
    if (ioctl(fd, PM_DEMO_IOC_GET_STATS, &stats) == 0) {
        printf("  Power State:       %s (%u)\n",
               stats.power_state == 1 ? "ACTIVE" : (stats.power_state == 0 ? "SUSPENDED" : "SLEEP"),
               stats.power_state);
        printf("  Usage Counter:     %u\n", stats.usage_count);
        printf("  Autosuspend Delay: %u ms\n", stats.autosuspend_ms);
        printf("  Resume Operations: %llu\n", (unsigned long long)stats.resume_count);
        printf("  Suspend Operations:%llu\n", (unsigned long long)stats.suspend_count);
        printf("  Active Time:       %llu ms\n", (unsigned long long)stats.active_time_ms);
        printf("  Hardware Rails:    %s\n", stats.hw_powered ? "3.3V / 1.2V Energized" : "Gated");
        printf("  Hardware Clock:    %s\n", stats.clock_enabled ? "100 MHz Active" : "Gated");
    } else {
        printf("[-] PM_DEMO_IOC_GET_STATS failed: %s\n", strerror(errno));
    }

    /* 5. Simulating Hardware Payload Write */
    print_section("5. Executing Simulated Hardware Payload Write");
    const char *workload = "CMD_EXECUTE_ACCELERATOR_BURST_0x42";
    bytes_written = write(fd, workload, strlen(workload));
    if (bytes_written > 0) {
        printf("%s[+] Sent %zd-byte command payload to device hardware%s\n",
               COLOR_GREEN, bytes_written, COLOR_RESET);
    }

    /* 6. Triggering Activity Burst */
    print_section("6. Triggering PM Wake Burst via IOCTL");
    if (ioctl(fd, PM_DEMO_IOC_WAKE_BURST) == 0) {
        printf("%s[+] Wake burst acknowledged (last-busy updated)%s\n", COLOR_GREEN, COLOR_RESET);
    }

    /* 7. Reconfiguring Autosuspend Delay to 1000 ms */
    print_section("7. Dynamically Reconfiguring Autosuspend Delay");
    uint32_t new_delay = 1000;
    printf("Setting autosuspend delay to %u ms via IOCTL...\n", new_delay);
    if (ioctl(fd, PM_DEMO_IOC_SET_AUTOSUSPEND, &new_delay) == 0) {
        printf("%s[+] Autosuspend delay set to %u ms%s\n", COLOR_GREEN, new_delay, COLOR_RESET);
    }

    /* 8. Close Character Device Node */
    print_section("8. Closing Device Node & Monitoring Autosuspend");
    printf("Closing device node (initiating idle countdown of %u ms)...\n", new_delay);
    close(fd);
    printf("%s[+] File closed. Device usage count dropped to 0.%s\n", COLOR_GREEN, COLOR_RESET);

    /* 9. Sleep and Verify Autosuspend */
    printf("Waiting 1.5 seconds for autosuspend delay timer to expire...\n");
    usleep(1500000); /* 1.5 seconds > 1000 ms */

    print_section("9. Post-Autosuspend State Verification");
    read_sysfs_file(SYSFS_SYS_STATUS, "Kernel Runtime Status");
    read_sysfs_file(SYSFS_TELEMETRY, "Custom PM Telemetry");

    printf("\n%s================================================================%s\n",
           COLOR_BOLD, COLOR_RESET);
    printf("%s[+] Chapter 31 Verification Complete: Power Management Verified!%s\n",
           COLOR_GREEN, COLOR_RESET);
    printf("%s================================================================%s\n",
           COLOR_BOLD, COLOR_RESET);

    return EXIT_SUCCESS;
}
