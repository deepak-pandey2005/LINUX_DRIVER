// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file test_client.c
 * @brief Chapter 13: User-space verification suite for Interrupt Handling
 * @author Deepak Pandey
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>

#include "interrupt_ioctl.h"

#define DEV_PATH "/dev/irq_demo"

#define COLOR_RESET  "\033[0m"
#define COLOR_GREEN  "\033[32m"
#define COLOR_RED    "\033[31m"
#define COLOR_BLUE   "\033[34m"
#define COLOR_YELLOW "\033[33m"

int main(void)
{
    int fd;
    struct irq_stats stats;
    int pass_count = 0;
    int test_count = 0;

    printf("%s=================================================================%s\n", COLOR_BLUE, COLOR_RESET);
    printf("%s       Linux Kernel Interrupt Subsystem Verification Suite       %s\n", COLOR_BLUE, COLOR_RESET);
    printf("%s=================================================================%s\n\n", COLOR_BLUE, COLOR_RESET);

    fd = open(DEV_PATH, O_RDWR);
    if (fd < 0) {
        printf("%s[FAIL]%s Unable to open %s: %s\n", COLOR_RED, COLOR_RESET, DEV_PATH, strerror(errno));
        printf("Did you load the module with 'make load'?\n");
        return 1;
    }

    /* Test 1: Query initial stats */
    test_count++;
    printf("[%sTEST %d: INITIAL IRQ QUERY%s]\n", COLOR_YELLOW, test_count, COLOR_RESET);
    if (ioctl(fd, IRQ_IOCTL_GET_STATS, &stats) == 0) {
        printf(" -> Target IRQ: %u\n", stats.irq_num);
        printf(" -> Hardware Registered: %s\n", stats.is_hardware_registered ? "YES" : "NO (Simulation Mode)");
        printf(" -> Initial Interrupt Count: %llu\n", stats.total_interrupts);
        printf("%s[PASS]%s Driver stats queried successfully.\n\n", COLOR_GREEN, COLOR_RESET);
        pass_count++;
    } else {
        perror("GET_STATS failed");
    }

    /* Test 2: Trigger Software Interrupts */
    test_count++;
    printf("[%sTEST %d: TRIGGERING SOFTWARE INTERRUPTS%s]\n", COLOR_YELLOW, test_count, COLOR_RESET);
    int trigger_burst = 50;
    printf(" Firing %d simulated interrupt events...\n", trigger_burst);
    for (int i = 0; i < trigger_burst; i++) {
        if (ioctl(fd, IRQ_IOCTL_TRIGGER_SOFTWARE, 0) < 0) {
            perror("TRIGGER_SOFTWARE failed");
            break;
        }
    }

    /* Verify count increased */
    if (ioctl(fd, IRQ_IOCTL_GET_STATS, &stats) == 0) {
        printf(" -> Post-burst Interrupt Count: %llu (Handled by CPU %u)\n",
               stats.total_interrupts, stats.last_cpu);
        if (stats.total_interrupts >= (__u64)trigger_burst) {
            printf("%s[PASS]%s Interrupt count successfully incremented by ISR!\n\n", COLOR_GREEN, COLOR_RESET);
            pass_count++;
        } else {
            printf("%s[FAIL]%s Expected at least %d interrupts, got %llu\n\n",
                   COLOR_RED, COLOR_RESET, trigger_burst, (unsigned long long)stats.total_interrupts);
        }
    }

    /* Test 3: Reset Stats */
    test_count++;
    printf("[%sTEST %d: RESETTING STATISTICS%s]\n", COLOR_YELLOW, test_count, COLOR_RESET);
    if (ioctl(fd, IRQ_IOCTL_RESET_STATS, 0) == 0) {
        ioctl(fd, IRQ_IOCTL_GET_STATS, &stats);
        printf(" -> Post-reset count: %llu\n", stats.total_interrupts);
        if (stats.total_interrupts == 0) {
            printf("%s[PASS]%s Statistics reset successfully.\n\n", COLOR_GREEN, COLOR_RESET);
            pass_count++;
        } else {
            printf("%s[FAIL]%s Count not reset to 0\n\n", COLOR_RED, COLOR_RESET);
        }
    } else {
        perror("RESET_STATS failed");
    }

    close(fd);

    printf("%s=================================================================%s\n", COLOR_BLUE, COLOR_RESET);
    printf(" Final Result: %s%d/%d passed%s\n",
           (pass_count == test_count) ? COLOR_GREEN : COLOR_RED,
           pass_count, test_count, COLOR_RESET);
    printf("%s=================================================================%s\n", COLOR_BLUE, COLOR_RESET);

    return (pass_count == test_count) ? 0 : 1;
}
