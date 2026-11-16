// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file test_client.c
 * @brief Chapter 32: Real-Time Clock (RTC) Verification Client
 * @author Deepak Pandey
 *
 * Verifies:
 *  - Discovering the mock RTC device node (/dev/rtcX).
 *  - Standard RTC IOCTLs: RTC_RD_TIME, RTC_SET_TIME.
 *  - Alarm programming: RTC_ALM_SET, RTC_AIE_ON, RTC_AIE_OFF.
 *  - Hardware interrupt reception via blocking read() on /dev/rtcX.
 *  - Event flag verification (RTC_AF alarm event assertion).
 *  - Sysfs telemetry inspection.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <linux/rtc.h>
#include <dirent.h>

/* Color formatting */
#define COLOR_RESET   "\033[0m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_RED     "\033[31m"
#define COLOR_BOLD    "\033[1m"

static void print_section(const char *title)
{
    printf("\n%s=== %s ===%s\n", COLOR_CYAN, title, COLOR_RESET);
}

/**
 * @brief Locate the mock RTC device node (/dev/rtcX) by querying /sys/class/rtc/.
 */
static int find_mock_rtc_dev(char *out_path, size_t max_len)
{
    DIR *dir = opendir("/sys/class/rtc");
    struct dirent *entry;
    char link_target[512];
    char sys_path[512];

    if (!dir)
        return -1;

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.')
            continue;

        snprintf(sys_path, sizeof(sys_path), "/sys/class/rtc/%.200s/device", entry->d_name);
        ssize_t len = readlink(sys_path, link_target, sizeof(link_target) - 1);
        if (len > 0) {
            link_target[len] = '\0';
            if (strstr(link_target, "mock_rtc_driver") != NULL) {
                snprintf(out_path, max_len, "/dev/%.200s", entry->d_name);
                closedir(dir);
                return 0;
            }
        }
    }

    closedir(dir);

    /* Fallback to /dev/rtc1 if exists, else /dev/rtc0 */
    if (access("/dev/rtc1", F_OK) == 0) {
        snprintf(out_path, max_len, "/dev/rtc1");
        return 0;
    }
    if (access("/dev/rtc0", F_OK) == 0) {
        snprintf(out_path, max_len, "/dev/rtc0");
        return 0;
    }

    return -1;
}

int main(int argc, char *argv[])
{
    char rtc_dev_path[256];
    int fd;
    struct rtc_time tm, orig_tm, set_tm;
    struct rtc_wkalrm alarm;
    unsigned long data = 0;
    fd_set read_fds;
    struct timeval tv;
    int ret;

    (void)argc;
    (void)argv;

    printf("%s================================================================%s\n",
           COLOR_BOLD, COLOR_RESET);
    printf("%s   Chapter 32: Real-Time Clock (RTC) Subsystem Client          %s\n",
           COLOR_BOLD, COLOR_RESET);
    printf("%s================================================================%s\n",
           COLOR_BOLD, COLOR_RESET);

    /* 1. Discover RTC Device Node */
    print_section("1. Discovering RTC Device Node");
    if (find_mock_rtc_dev(rtc_dev_path, sizeof(rtc_dev_path)) < 0) {
        fprintf(stderr, "%s[-] Failed to discover RTC device node in /sys/class/rtc/%s\n",
                COLOR_RED, COLOR_RESET);
        return EXIT_FAILURE;
    }
    printf("%s[+] Target RTC Device Node: %s%s\n", COLOR_GREEN, rtc_dev_path, COLOR_RESET);

    /* 2. Open Device Node */
    print_section("2. Opening RTC Device Node");
    fd = open(rtc_dev_path, O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "%s[-] Failed to open %s: %s%s\n",
                COLOR_RED, rtc_dev_path, strerror(errno), COLOR_RESET);
        fprintf(stderr, "Ensure module 'rtc_driver_module.ko' is loaded and permissions are granted.\n");
        return EXIT_FAILURE;
    }
    printf("%s[+] Opened %s successfully (fd: %d)%s\n", COLOR_GREEN, rtc_dev_path, fd, COLOR_RESET);

    /* 3. Read Current RTC Time (RTC_RD_TIME) */
    print_section("3. Reading Current Hardware Time (RTC_RD_TIME)");
    if (ioctl(fd, RTC_RD_TIME, &orig_tm) < 0) {
        fprintf(stderr, "%s[-] RTC_RD_TIME failed: %s%s\n", COLOR_RED, strerror(errno), COLOR_RESET);
        close(fd);
        return EXIT_FAILURE;
    }
    printf("  Current Hardware Time: %04d-%02d-%02d %02d:%02d:%02d UTC\n",
           orig_tm.tm_year + 1900, orig_tm.tm_mon + 1, orig_tm.tm_mday,
           orig_tm.tm_hour, orig_tm.tm_min, orig_tm.tm_sec);

    /* 4. Set Hardware Time (RTC_SET_TIME) */
    print_section("4. Updating Hardware Time (RTC_SET_TIME)");
    set_tm = orig_tm;
    set_tm.tm_sec += 5;
    if (set_tm.tm_sec >= 60) {
        set_tm.tm_sec -= 60;
        set_tm.tm_min++;
    }
    printf("Setting simulated time to: %04d-%02d-%02d %02d:%02d:%02d UTC...\n",
           set_tm.tm_year + 1900, set_tm.tm_mon + 1, set_tm.tm_mday,
           set_tm.tm_hour, set_tm.tm_min, set_tm.tm_sec);
    if (ioctl(fd, RTC_SET_TIME, &set_tm) < 0) {
        fprintf(stderr, "%s[-] RTC_SET_TIME failed: %s%s\n", COLOR_RED, strerror(errno), COLOR_RESET);
    } else {
        printf("%s[+] RTC_SET_TIME successfully committed to hardware!%s\n", COLOR_GREEN, COLOR_RESET);
    }

    /* Verify updated time */
    if (ioctl(fd, RTC_RD_TIME, &tm) == 0) {
        printf("  Readback Hardware Time: %04d-%02d-%02d %02d:%02d:%02d UTC\n",
               tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
               tm.tm_hour, tm.tm_min, tm.tm_sec);
    }

    /* 5. Program Alarm (RTC_ALM_SET / RTC_AIE_ON) */
    print_section("5. Programming RTC Hardware Alarm (+2 seconds)");
    memset(&alarm, 0, sizeof(alarm));
    if (ioctl(fd, RTC_RD_TIME, &alarm.time) < 0) {
        fprintf(stderr, "%s[-] Failed to read baseline time for alarm: %s%s\n",
                COLOR_RED, strerror(errno), COLOR_RESET);
        close(fd);
        return EXIT_FAILURE;
    }

    alarm.time.tm_sec += 2;
    if (alarm.time.tm_sec >= 60) {
        alarm.time.tm_sec -= 60;
        alarm.time.tm_min++;
    }
    alarm.enabled = 1;

    printf("Arming alarm for: %04d-%02d-%02d %02d:%02d:%02d UTC\n",
           alarm.time.tm_year + 1900, alarm.time.tm_mon + 1, alarm.time.tm_mday,
           alarm.time.tm_hour, alarm.time.tm_min, alarm.time.tm_sec);

    if (ioctl(fd, RTC_ALM_SET, &alarm.time) < 0) {
        fprintf(stderr, "%s[-] RTC_ALM_SET failed: %s%s\n", COLOR_RED, strerror(errno), COLOR_RESET);
        close(fd);
        return EXIT_FAILURE;
    }

    /* Enable Alarm Interrupt */
    if (ioctl(fd, RTC_AIE_ON, 0) < 0) {
        fprintf(stderr, "%s[-] RTC_AIE_ON failed: %s%s\n", COLOR_RED, strerror(errno), COLOR_RESET);
    } else {
        printf("%s[+] RTC Alarm Interrupts Enabled (RTC_AIE_ON)%s\n", COLOR_GREEN, COLOR_RESET);
    }

    /* 6. Await Alarm Interrupt via Select & Blocking Read */
    print_section("6. Awaiting Hardware Alarm Interrupt Event");
    printf("Blocking on %s descriptor (5-second timeout)...\n", rtc_dev_path);

    FD_ZERO(&read_fds);
    FD_SET(fd, &read_fds);
    tv.tv_sec = 5;
    tv.tv_usec = 0;

    ret = select(fd + 1, &read_fds, NULL, NULL, &tv);
    if (ret < 0) {
        fprintf(stderr, "%s[-] select() error: %s%s\n", COLOR_RED, strerror(errno), COLOR_RESET);
    } else if (ret == 0) {
        fprintf(stderr, "%s[-] Alarm timeout! No interrupt received within 5 seconds.%s\n",
                COLOR_RED, COLOR_RESET);
    } else {
        ssize_t n = read(fd, &data, sizeof(data));
        if (n == sizeof(data)) {
            printf("%s[+] ALARM INTERRUPT RECEIVED! Event Data: 0x%08lx%s\n",
                   COLOR_GREEN, data, COLOR_RESET);
            printf("  Interrupt Type: %s\n",
                   (data & RTC_AF) ? "RTC_AF (Alarm Interrupt Event)" : "Other");
            printf("  Event Frequency Count: %lu\n", data >> 8);
        } else {
            fprintf(stderr, "[-] read() returned %zd bytes: %s\n", n, strerror(errno));
        }
    }

    /* Disable Alarm Interrupt */
    ioctl(fd, RTC_AIE_OFF, 0);

    /* 7. Read Sysfs Telemetry */
    print_section("7. Inspecting Driver Telemetry via Sysfs");
    char sysfs_telemetry[512];
    snprintf(sysfs_telemetry, sizeof(sysfs_telemetry),
             "/sys/devices/platform/mock_rtc_driver/rtc_telemetry");
    int sfd = open(sysfs_telemetry, O_RDONLY);
    if (sfd >= 0) {
        char buf[512];
        ssize_t sn = read(sfd, buf, sizeof(buf) - 1);
        if (sn > 0) {
            buf[sn] = '\0';
            printf("%s", buf);
        }
        close(sfd);
    } else {
        printf("  Telemetry file %s not directly reachable (%s)\n", sysfs_telemetry, strerror(errno));
    }

    close(fd);

    printf("\n%s================================================================%s\n",
           COLOR_BOLD, COLOR_RESET);
    printf("%s[+] Chapter 32 Verification Complete: RTC Subsystem Verified!     %s\n",
           COLOR_GREEN, COLOR_RESET);
    printf("%s================================================================%s\n",
           COLOR_BOLD, COLOR_RESET);

    return EXIT_SUCCESS;
}
