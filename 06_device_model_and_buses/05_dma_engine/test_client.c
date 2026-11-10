// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file test_client.c
 * @brief Chapter 20: User-space verification suite for DMA Subsystem
 * @author Deepak Pandey
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>

#include "dma_ioctl.h"

#define DEV_PATH "/dev/dma_engine_demo"

#define COLOR_RESET  "\033[0m"
#define COLOR_GREEN  "\033[32m"
#define COLOR_RED    "\033[31m"
#define COLOR_BLUE   "\033[34m"
#define COLOR_YELLOW "\033[33m"

int main(void)
{
    int fd;
    struct dma_status_info info;
    char text_buf[512];
    ssize_t bytes;
    int pass_count = 0;
    int test_count = 0;

    printf("%s=================================================================%s\n", COLOR_BLUE, COLOR_RESET);
    printf("%s         Linux DMA Engine & Streaming Verification Suite         %s\n", COLOR_BLUE, COLOR_RESET);
    printf("%s=================================================================%s\n\n", COLOR_BLUE, COLOR_RESET);

    fd = open(DEV_PATH, O_RDWR);
    if (fd < 0) {
        printf("%s[FAIL]%s Unable to open %s: %s\n", COLOR_RED, COLOR_RESET, DEV_PATH, strerror(errno));
        printf("Did you run 'make load'?\n");
        return 1;
    }

    /* Test 1: Query DMA Mapping Status via IOCTL */
    test_count++;
    printf("[%sTEST %d: INSPECTING DMA BUS ADDRESSES AND BUFFERS%s]\n", COLOR_YELLOW, test_count, COLOR_RESET);
    if (ioctl(fd, DMA_IOCTL_GET_STATUS, &info) == 0) {
        printf(" -> Coherent DMA Bus Addr : 0x%llx (Size: %u bytes)\n",
               (unsigned long long)info.coherent_bus_addr, info.coherent_size);
        printf(" -> Streaming DMA Bus Addr: 0x%llx (Size: %u bytes)\n",
               (unsigned long long)info.streaming_bus_addr, info.streaming_size);
        printf(" -> Total Prior Transfers : %llu\n", (unsigned long long)info.total_dma_transfers);

        if (info.coherent_bus_addr != 0 && info.streaming_bus_addr != 0) {
            printf("%s[PASS]%s Coherent and Streaming DMA mappings established!\n\n", COLOR_GREEN, COLOR_RESET);
            pass_count++;
        } else {
            printf("%s[FAIL]%s Null DMA bus address encountered.\n\n", COLOR_RED, COLOR_RESET);
        }
    } else {
        perror("DMA_IOCTL_GET_STATUS failed");
    }

    /* Test 2: Trigger Simulated DMA Engine Transfers */
    test_count++;
    printf("[%sTEST %d: EXECUTING HIGH-SPEED DMA BURST TRANSFERS%s]\n", COLOR_YELLOW, test_count, COLOR_RESET);
    int iterations = 10;
    for (int i = 0; i < iterations; i++) {
        if (ioctl(fd, DMA_IOCTL_START_TRANSFER, 0) < 0) {
            perror("DMA_IOCTL_START_TRANSFER failed");
            break;
        }
    }

    ioctl(fd, DMA_IOCTL_GET_STATUS, &info);
    printf(" -> Total DMA Transfers: %llu\n", (unsigned long long)info.total_dma_transfers);
    printf(" -> Last Transfer Timestamp: %llu ns\n", (unsigned long long)info.last_transfer_time_ns);
    if (info.total_dma_transfers >= (__u64)iterations) {
        printf("%s[PASS]%s DMA memory engine transfers completed without bus faults!\n\n", COLOR_GREEN, COLOR_RESET);
        pass_count++;
    } else {
        printf("%s[FAIL]%s Transfer count mismatch.\n\n", COLOR_RED, COLOR_RESET);
    }

    /* Test 3: Read Diagnostic Report */
    test_count++;
    printf("[%sTEST %d: READING DMA REPORT%s]\n", COLOR_YELLOW, test_count, COLOR_RESET);
    bytes = read(fd, text_buf, sizeof(text_buf) - 1);
    if (bytes > 0) {
        text_buf[bytes] = '\0';
        printf("%s\n", text_buf);
        if (strstr(text_buf, "Coherent DMA Bus") != NULL && strstr(text_buf, "Streaming DMA Bus") != NULL) {
            printf("%s[PASS]%s Diagnostic report read successfully.\n\n", COLOR_GREEN, COLOR_RESET);
            pass_count++;
        } else {
            printf("%s[FAIL]%s Unexpected report content.\n\n", COLOR_RED, COLOR_RESET);
        }
    } else {
        printf("%s[FAIL]%s Read returned 0 bytes.\n\n", COLOR_RED, COLOR_RESET);
    }

    close(fd);

    printf("%s=================================================================%s\n", COLOR_BLUE, COLOR_RESET);
    printf(" Final Result: %s%d/%d passed%s\n",
           (pass_count == test_count) ? COLOR_GREEN : COLOR_RED,
           pass_count, test_count, COLOR_RESET);
    printf("%s=================================================================%s\n", COLOR_BLUE, COLOR_RESET);

    return (pass_count == test_count) ? 0 : 1;
}

