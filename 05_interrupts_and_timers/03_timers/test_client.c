// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file test_client.c
 * @brief Chapter 15: User-space verification suite for Kernel Timers
 * @author Deepak Pandey
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>

#include "timer_ioctl.h"

#define DEV_PATH "/dev/kernel_timers"

#define COLOR_RESET  "\033[0m"
#define COLOR_GREEN  "\033[32m"
#define COLOR_RED    "\033[31m"
#define COLOR_BLUE   "\033[34m"
#define COLOR_YELLOW "\033[33m"

int main(void)
{
    int fd;
    struct timer_stats stats;
    int pass_count = 0;
    int test_count = 0;

    printf("%s=================================================================%s\n", COLOR_BLUE, COLOR_RESET);
    printf("%s       Linux Kernel Timers & Jiffies Verification Suite          %s\n", COLOR_BLUE, COLOR_RESET);
    printf("%s=================================================================%s\n\n", COLOR_BLUE, COLOR_RESET);

    fd = open(DEV_PATH, O_RDWR);
    if (fd < 0) {
        printf("%s[FAIL]%s Unable to open %s: %s\n", COLOR_RED, COLOR_RESET, DEV_PATH, strerror(errno));
        printf("Did you load the module with 'make load'?\n");
        return 1;
    }

    /* Test 1: Query initial stats and kernel HZ rate */
    test_count++;
    printf("[%sTEST %d: KERNEL HZ CONFIGURATION%s]\n", COLOR_YELLOW, test_count, COLOR_RESET);
    if (ioctl(fd, TIMER_IOCTL_GET_STATS, &stats) == 0) {
        printf(" -> Kernel Timer Frequency (HZ): %u ticks/second (1 tick = %.2f ms)\n",
               stats.hz_rate, 1000.0 / stats.hz_rate);
        printf("%s[PASS]%s Kernel timer configuration queried.\n\n", COLOR_GREEN, COLOR_RESET);
        pass_count++;
    } else {
        perror("GET_STATS failed");
    }

    /* Test 2: Standard Low-Resolution Timer (100ms interval for 400ms) */
    test_count++;
    printf("[%sTEST %d: LOW-RESOLUTION TIMER (timer_list)%s]\n", COLOR_YELLOW, test_count, COLOR_RESET);
    __u32 std_period_ms = 100;
    printf(" Starting standard periodic timer at %u ms period...\n", std_period_ms);
    if (ioctl(fd, TIMER_IOCTL_START_STD, &std_period_ms) == 0) {
        /* Sleep 450 ms: Expecting ~4 ticks */
        usleep(450000);
        ioctl(fd, TIMER_IOCTL_STOP_STD, 0);
        ioctl(fd, TIMER_IOCTL_GET_STATS, &stats);
        printf(" -> Standard timer ticks observed: %u\n", stats.std_timer_ticks);
        if (stats.std_timer_ticks >= 3 && stats.std_timer_ticks <= 6) {
            printf("%s[PASS]%s Standard kernel timer fired at expected periodic frequency!\n\n",
                   COLOR_GREEN, COLOR_RESET);
            pass_count++;
        } else {
            printf("%s[FAIL]%s Unexpected tick count: %u (expected ~4)\n\n", COLOR_RED, COLOR_RESET, stats.std_timer_ticks);
        }
    } else {
        perror("START_STD failed");
    }

    /* Test 3: High-Resolution Timer (hrtimer: 1000us = 1ms interval for 100ms) */
    test_count++;
    printf("[%sTEST %d: HIGH-RESOLUTION TIMER (hrtimer)%s]\n", COLOR_YELLOW, test_count, COLOR_RESET);
    __u32 hr_period_us = 1000; /* 1 ms */
    printf(" Starting hrtimer with sub-millisecond precision (%u us period)...\n", hr_period_us);
    if (ioctl(fd, TIMER_IOCTL_START_HR, &hr_period_us) == 0) {
        /* Sleep 100 ms: Expecting ~90-110 ticks */
        usleep(100000);
        ioctl(fd, TIMER_IOCTL_STOP_HR, 0);
        ioctl(fd, TIMER_IOCTL_GET_STATS, &stats);
        printf(" -> High-resolution timer ticks observed: %u\n", stats.hr_timer_ticks);
        if (stats.hr_timer_ticks >= 70 && stats.hr_timer_ticks <= 130) {
            printf("%s[PASS]%s High-resolution timer delivered ultra-fast sub-millisecond precision!\n\n",
                   COLOR_GREEN, COLOR_RESET);
            pass_count++;
        } else {
            printf("%s[FAIL]%s Unexpected hrtimer tick count: %u\n\n", COLOR_RED, COLOR_RESET, stats.hr_timer_ticks);
        }
    } else {
        perror("START_HR failed");
    }

    close(fd);

    printf("%s=================================================================%s\n", COLOR_BLUE, COLOR_RESET);
    printf(" Final Result: %s%d/%d passed%s\n",
           (pass_count == test_count) ? COLOR_GREEN : COLOR_RED,
           pass_count, test_count, COLOR_RESET);
    printf("%s=================================================================%s\n", COLOR_BLUE, COLOR_RESET);

    return (pass_count == test_count) ? 0 : 1;
}
