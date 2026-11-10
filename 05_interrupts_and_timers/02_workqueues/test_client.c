// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file test_client.c
 * @brief Chapter 14: User-space verification suite for Workqueues Subsystem
 * @author Deepak Pandey
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>

#include "workqueue_ioctl.h"

#define DEV_PATH "/dev/workqueue_demo"

#define COLOR_RESET  "\033[0m"
#define COLOR_GREEN  "\033[32m"
#define COLOR_RED    "\033[31m"
#define COLOR_BLUE   "\033[34m"
#define COLOR_YELLOW "\033[33m"

int main(void)
{
    int fd;
    struct wq_stats stats;
    int pass_count = 0;
    int test_count = 0;

    printf("%s=================================================================%s\n", COLOR_BLUE, COLOR_RESET);
    printf("%s        Linux Kernel Workqueue Subsystem Verification            %s\n", COLOR_BLUE, COLOR_RESET);
    printf("%s=================================================================%s\n\n", COLOR_BLUE, COLOR_RESET);

    fd = open(DEV_PATH, O_RDWR);
    if (fd < 0) {
        printf("%s[FAIL]%s Unable to open %s: %s\n", COLOR_RED, COLOR_RESET, DEV_PATH, strerror(errno));
        printf("Did you load the module with 'make load'?\n");
        return 1;
    }

    /* Test 1: Trigger Default System Workqueue */
    test_count++;
    printf("[%sTEST %d: DEFAULT SYSTEM WORKQUEUE%s]\n", COLOR_YELLOW, test_count, COLOR_RESET);
    if (ioctl(fd, WQ_IOCTL_TRIGGER_DEFAULT, 0) == 0) {
        usleep(50000); /* 50ms to allow kworker to execute */
        ioctl(fd, WQ_IOCTL_GET_STATS, &stats);
        printf(" -> Default work executed: %u times (by kworker PID %u)\n",
               stats.default_work_count, stats.last_worker_pid);
        if (stats.default_work_count >= 1) {
            printf("%s[PASS]%s System workqueue executed task asynchronously.\n\n", COLOR_GREEN, COLOR_RESET);
            pass_count++;
        } else {
            printf("%s[FAIL]%s Work was not executed.\n\n", COLOR_RED, COLOR_RESET);
        }
    } else {
        perror("TRIGGER_DEFAULT failed");
    }

    /* Test 2: Trigger Custom Dedicated Workqueue */
    test_count++;
    printf("[%sTEST %d: DEDICATED PRIVATE WORKQUEUE%s]\n", COLOR_YELLOW, test_count, COLOR_RESET);
    if (ioctl(fd, WQ_IOCTL_TRIGGER_CUSTOM, 0) == 0) {
        usleep(50000);
        ioctl(fd, WQ_IOCTL_GET_STATS, &stats);
        printf(" -> Custom work executed: %u times (by kworker PID %u)\n",
               stats.custom_work_count, stats.last_worker_pid);
        if (stats.custom_work_count >= 1) {
            printf("%s[PASS]%s Custom private workqueue successfully processed task.\n\n", COLOR_GREEN, COLOR_RESET);
            pass_count++;
        } else {
            printf("%s[FAIL]%s Custom work was not executed.\n\n", COLOR_RED, COLOR_RESET);
        }
    } else {
        perror("TRIGGER_CUSTOM failed");
    }

    /* Test 3: Delayed Work (with Execution) */
    test_count++;
    printf("[%sTEST %d: DELAYED WORK (SCHEDULE & RUN)%s]\n", COLOR_YELLOW, test_count, COLOR_RESET);
    __u32 delay_ms = 100;
    printf(" Scheduling delayed work with %u ms timeout...\n", delay_ms);
    if (ioctl(fd, WQ_IOCTL_TRIGGER_DELAYED, &delay_ms) == 0) {
        /* Sleep 150ms to ensure the timer expired and work ran */
        usleep(150000);
        ioctl(fd, WQ_IOCTL_GET_STATS, &stats);
        printf(" -> Delayed work executed: %u times\n", stats.delayed_work_count);
        if (stats.delayed_work_count >= 1) {
            printf("%s[PASS]%s Delayed work executed after timer expiration.\n\n", COLOR_GREEN, COLOR_RESET);
            pass_count++;
        } else {
            printf("%s[FAIL]%s Delayed work failed to execute.\n\n", COLOR_RED, COLOR_RESET);
        }
    } else {
        perror("TRIGGER_DELAYED failed");
    }

    /* Test 4: Delayed Work (Cancellation before expiration) */
    test_count++;
    printf("[%sTEST %d: DELAYED WORK CANCELLATION%s]\n", COLOR_YELLOW, test_count, COLOR_RESET);
    delay_ms = 500;
    printf(" Scheduling delayed work with %u ms timeout, then immediately canceling...\n", delay_ms);
    ioctl(fd, WQ_IOCTL_TRIGGER_DELAYED, &delay_ms);
    usleep(10000); /* 10ms */
    int cancel_res = ioctl(fd, WQ_IOCTL_CANCEL_DELAYED, 0);
    printf(" -> cancel_delayed_work_sync returned: %d\n", cancel_res);
    if (cancel_res == 0) {
        printf("%s[PASS]%s Successfully canceled in-flight delayed work before execution.\n\n", COLOR_GREEN, COLOR_RESET);
        pass_count++;
    } else {
        printf("%s[FAIL]%s Cancellation failed or work already ran.\n\n", COLOR_RED, COLOR_RESET);
    }

    close(fd);

    printf("%s=================================================================%s\n", COLOR_BLUE, COLOR_RESET);
    printf(" Final Result: %s%d/%d passed%s\n",
           (pass_count == test_count) ? COLOR_GREEN : COLOR_RED,
           pass_count, test_count, COLOR_RESET);
    printf("%s=================================================================%s\n", COLOR_BLUE, COLOR_RESET);

    return (pass_count == test_count) ? 0 : 1;
}

