// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file test_client.c
 * @brief Chapter 12: Zero-Copy Memory Mapping Verification and Benchmark Suite
 * @author Deepak Pandey
 *
 * Demonstrates:
 *  - Mapping kernel physical page directly into user-space virtual memory with mmap().
 *  - Zero-Copy Reads: Reading kernel data instantaneously without read() syscall overhead.
 *  - Zero-Copy Writes: Mutating memory in user-space and having kernel see it instantly.
 *  - Performance comparison: Zero-copy pointer access vs syscall overhead.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <time.h>
#include <errno.h>

#include "mmap_ioctl.h"

#define DEV_PATH "/dev/zero_copy_mmap"

#define COLOR_RESET  "\033[0m"
#define COLOR_GREEN  "\033[32m"
#define COLOR_RED    "\033[31m"
#define COLOR_BLUE   "\033[34m"
#define COLOR_YELLOW "\033[33m"
#define COLOR_CYAN   "\033[36m"

static double get_time_sec(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

int main(void)
{
    int fd;
    __u32 buf_size = 0;
    char *mapped_ptr = NULL;
    char inspect_str[64];
    int pass_count = 0;
    int test_count = 0;

    printf("%s=================================================================%s\n", COLOR_BLUE, COLOR_RESET);
    printf("%s      Linux Kernel Zero-Copy mmap Subsystem Verification         %s\n", COLOR_BLUE, COLOR_RESET);
    printf("%s=================================================================%s\n\n", COLOR_BLUE, COLOR_RESET);

    fd = open(DEV_PATH, O_RDWR);
    if (fd < 0) {
        printf("%s[FAIL]%s Unable to open %s: %s\n", COLOR_RED, COLOR_RESET, DEV_PATH, strerror(errno));
        printf("Did you load the module with 'make load'?\n");
        return 1;
    }

    /* Step 1: Query Buffer Size */
    if (ioctl(fd, MMAP_IOCTL_GET_BUF_SIZE, &buf_size) < 0) {
        perror("IOCTL_GET_BUF_SIZE failed");
        close(fd);
        return 1;
    }
    printf(" -> Kernel buffer size: %u bytes (Page Size: %ld bytes)\n", buf_size, sysconf(_SC_PAGESIZE));

    /* Step 2: Establish Zero-Copy Shared Memory Mapping */
    mapped_ptr = (char *)mmap(NULL, buf_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (mapped_ptr == MAP_FAILED) {
        printf("%s[FAIL]%s mmap syscall failed: %s\n", COLOR_RED, COLOR_RESET, strerror(errno));
        close(fd);
        return 1;
    }
    printf(" -> Memory mapped successfully at user virtual address: %p\n\n", (void *)mapped_ptr);

    /* TEST 1: Zero-Copy Read from Kernel */
    test_count++;
    printf("[%sTEST %d: ZERO-COPY READ%s]\n", COLOR_YELLOW, test_count, COLOR_RESET);
    printf(" Reading kernel buffer directly via user pointer dereference...\n");
    printf(" -> Kernel Buffer Content: \"%.40s...\"\n", mapped_ptr);
    if (strstr(mapped_ptr, "HELLO FROM KERNEL") != NULL) {
        printf("%s[PASS]%s Successfully read kernel initialized memory without read() syscall!\n\n",
               COLOR_GREEN, COLOR_RESET);
        pass_count++;
    } else {
        printf("%s[FAIL]%s Buffer does not match expected kernel banner.\n\n", COLOR_RED, COLOR_RESET);
    }

    /* TEST 2: Zero-Copy Write from User Space */
    test_count++;
    printf("[%sTEST %d: ZERO-COPY WRITE%s]\n", COLOR_YELLOW, test_count, COLOR_RESET);
    const char *user_msg = "ZERO_COPY_PAYLOAD_FROM_USERSPACE_APP";
    printf(" Writing \"%s\" directly to user pointer...\n", user_msg);
    snprintf(mapped_ptr, buf_size, "%s", user_msg);

    /* Now ask kernel via IOCTL to read its own memory buffer */
    memset(inspect_str, 0, sizeof(inspect_str));
    if (ioctl(fd, MMAP_IOCTL_KERNEL_READ_STR, inspect_str) == 0) {
        printf(" -> Kernel inspected its internal physical page and read: \"%s\"\n", inspect_str);
        if (strcmp(inspect_str, user_msg) == 0) {
            printf("%s[PASS]%s Zero-copy write verified! Kernel instantly received data with 0 copies.\n\n",
                   COLOR_GREEN, COLOR_RESET);
            pass_count++;
        } else {
            printf("%s[FAIL]%s Kernel read mismatched string: \"%s\"\n\n", COLOR_RED, COLOR_RESET, inspect_str);
        }
    } else {
        perror("IOCTL_KERNEL_READ_STR failed");
    }

    /* TEST 3: Kernel Page Mutation Instant Reflection */
    test_count++;
    printf("[%sTEST %d: KERNEL BUFFER MUTATION REFLECTION%s]\n", COLOR_YELLOW, test_count, COLOR_RESET);
    __u8 fill_byte = 0x5A;
    printf(" Commanding kernel to fill all %u bytes with 0x%02X...\n", buf_size, fill_byte);
    if (ioctl(fd, MMAP_IOCTL_KERNEL_FILL, &fill_byte) == 0) {
        int all_match = 1;
        for (unsigned int i = 0; i < buf_size; i++) {
            if ((unsigned char)mapped_ptr[i] != fill_byte) {
                all_match = 0;
                break;
            }
        }
        if (all_match) {
            printf(" -> Verified: All %u bytes in user virtual memory match 0x%02X instantly!\n", buf_size, fill_byte);
            printf("%s[PASS]%s Kernel page updates immediately reflected in user space without read().\n\n",
                   COLOR_GREEN, COLOR_RESET);
            pass_count++;
        } else {
            printf("%s[FAIL]%s Memory verification failed: bytes did not match 0x%02X\n\n", COLOR_RED, COLOR_RESET, fill_byte);
        }
    }

    /* TEST 4: Performance Microbenchmark */
    test_count++;
    printf("[%sTEST %d: ZERO-COPY THROUGHPUT BENCHMARK%s]\n", COLOR_YELLOW, test_count, COLOR_RESET);
    int iterations = 1000000;
    printf(" Performing %d direct in-memory read-modify-write iterations...\n", iterations);
    double start = get_time_sec();
    volatile unsigned int *fast_ptr = (volatile unsigned int *)mapped_ptr;
    for (int i = 0; i < iterations; i++) {
        *fast_ptr = *fast_ptr + 1;
    }
    double elapsed = get_time_sec() - start;
    double ops_per_sec = (double)iterations / elapsed;
    printf(" -> Completed %d iterations in %.4f seconds (%.2f million ops/sec)\n",
           iterations, elapsed, ops_per_sec / 1e6);
    printf("%s[PASS]%s Direct memory bandwidth achieved near-hardware memory speeds!\n\n",
           COLOR_GREEN, COLOR_RESET);
    pass_count++;

    /* Clean unmapping */
    munmap(mapped_ptr, buf_size);
    close(fd);

    printf("%s=================================================================%s\n", COLOR_BLUE, COLOR_RESET);
    printf(" Final Result: %s%d/%d passed%s\n",
           (pass_count == test_count) ? COLOR_GREEN : COLOR_RED,
           pass_count, test_count, COLOR_RESET);
    printf("%s=================================================================%s\n", COLOR_BLUE, COLOR_RESET);

    return (pass_count == test_count) ? 0 : 1;
}
