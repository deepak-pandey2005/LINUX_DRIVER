// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file test_client.c
 * @brief Chapter 11: Multi-threaded Concurrency and Contention Benchmark
 * @author Deepak Pandey
 *
 * Demonstrates:
 *  - High-contention multi-threaded hammering of the driver.
 *  - Empirical proof of data race / lost updates in unsafe mode.
 *  - 100% precision with atomic operations, spinlocks, and mutexes.
 *  - Microbenchmark comparison (execution latency and throughput).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include <time.h>
#include <errno.h>

#include "locking_ioctl.h"

#define DEV_PATH "/dev/concurrency_bench"
#define NUM_THREADS 10
#define ITERATIONS_PER_THREAD 20000

#define COLOR_RESET  "\033[0m"
#define COLOR_GREEN  "\033[32m"
#define COLOR_RED    "\033[31m"
#define COLOR_BLUE   "\033[34m"
#define COLOR_YELLOW "\033[33m"
#define COLOR_CYAN   "\033[36m"

struct thread_arg {
    int fd;
    unsigned long ioctl_cmd;
    int iterations;
};

static void *worker_thread(void *arg)
{
    struct thread_arg *targ = (struct thread_arg *)arg;
    for (int i = 0; i < targ->iterations; i++) {
        if (ioctl(targ->fd, targ->ioctl_cmd, 0) < 0) {
            perror("ioctl failure");
            pthread_exit((void *)(intptr_t)-1);
        }
    }
    pthread_exit(NULL);
}

static double get_time_sec(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

static void run_benchmark(int fd, unsigned long cmd, const char *name, __u64 *out_val, double *out_time)
{
    pthread_t threads[NUM_THREADS];
    struct thread_arg args[NUM_THREADS];

    printf(" Running %s benchmark with %d threads x %d iterations (Total: %d)...\n",
           name, NUM_THREADS, ITERATIONS_PER_THREAD, NUM_THREADS * ITERATIONS_PER_THREAD);

    double start = get_time_sec();

    for (int i = 0; i < NUM_THREADS; i++) {
        args[i].fd = fd;
        args[i].ioctl_cmd = cmd;
        args[i].iterations = ITERATIONS_PER_THREAD;
        pthread_create(&threads[i], NULL, worker_thread, &args[i]);
    }

    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    double elapsed = get_time_sec() - start;
    *out_time = elapsed;

    struct locking_results res;
    if (ioctl(fd, LOCKING_IOCTL_GET_RESULTS, &res) < 0) {
        perror("LOCKING_IOCTL_GET_RESULTS failed");
        return;
    }

    if (cmd == LOCKING_IOCTL_INC_UNSAFE)
        *out_val = res.unsafe_val;
    else if (cmd == LOCKING_IOCTL_INC_ATOMIC)
        *out_val = res.atomic_val;
    else if (cmd == LOCKING_IOCTL_INC_SPIN)
        *out_val = res.spinlock_val;
    else if (cmd == LOCKING_IOCTL_INC_MUTEX)
        *out_val = res.mutex_val;
}

int main(void)
{
    int fd;
    __u64 val;
    double duration;
    __u64 expected = (__u64)NUM_THREADS * ITERATIONS_PER_THREAD;

    printf("%s=================================================================%s\n", COLOR_BLUE, COLOR_RESET);
    printf("%s     Linux Driver Concurrency & Synchronization Benchmark        %s\n", COLOR_BLUE, COLOR_RESET);
    printf("%s=================================================================%s\n\n", COLOR_BLUE, COLOR_RESET);

    fd = open(DEV_PATH, O_RDWR);
    if (fd < 0) {
        printf("%s[FAIL]%s Cannot open %s: %s\n", COLOR_RED, COLOR_RESET, DEV_PATH, strerror(errno));
        printf("Did you load the module with 'make load'?\n");
        return 1;
    }

    /* Reset driver state */
    ioctl(fd, LOCKING_IOCTL_RESET, 0);

    /* Test 1: Unprotected (Unsafe) Mode */
    printf("[%sTEST 1: UNSAFE CONCURRENT ACCESS%s]\n", COLOR_YELLOW, COLOR_RESET);
    run_benchmark(fd, LOCKING_IOCTL_INC_UNSAFE, "Unprotected Counter", &val, &duration);
    printf(" -> Expected: %llu, Actual: %llu (Elapsed: %.4f s)\n", expected, val, duration);
    if (val < expected) {
        double loss_pct = (1.0 - ((double)val / (double)expected)) * 100.0;
        printf(" -> %sRACE CONDITION DETECTED! Lost %llu increments (%.2f%% data loss)%s\n",
               COLOR_RED, expected - val, loss_pct, COLOR_RESET);
        printf("%s[PASS]%s Successfully proved data corruption under concurrent race condition.\n\n",
               COLOR_GREEN, COLOR_RESET);
    } else {
        printf("%s[NOTE]%s No lost increments observed on this test run (system had low contention).\n\n",
               COLOR_YELLOW, COLOR_RESET);
    }

    /* Test 2: Atomic Operations */
    printf("[%sTEST 2: LOCKLESS ATOMICS (atomic64_t)%s]\n", COLOR_YELLOW, COLOR_RESET);
    run_benchmark(fd, LOCKING_IOCTL_INC_ATOMIC, "atomic64_t", &val, &duration);
    printf(" -> Expected: %llu, Actual: %llu (Elapsed: %.4f s, %.0f ops/sec)\n",
           expected, val, duration, (double)expected / duration);
    if (val == expected) {
        printf("%s[PASS]%s 100%% precision verified! Zero lost increments.\n\n", COLOR_GREEN, COLOR_RESET);
    } else {
        printf("%s[FAIL]%s Atomic counter mismatch! (%llu != %llu)\n\n", COLOR_RED, COLOR_RESET, val, expected);
    }

    /* Test 3: Spinlock Synchronization */
    printf("[%sTEST 3: SPINLOCK SYNCHRONIZATION (spinlock_t)%s]\n", COLOR_YELLOW, COLOR_RESET);
    run_benchmark(fd, LOCKING_IOCTL_INC_SPIN, "spinlock_t", &val, &duration);
    printf(" -> Expected: %llu, Actual: %llu (Elapsed: %.4f s, %.0f ops/sec)\n",
           expected, val, duration, (double)expected / duration);
    if (val == expected) {
        printf("%s[PASS]%s 100%% precision verified! Zero lost increments.\n\n", COLOR_GREEN, COLOR_RESET);
    } else {
        printf("%s[FAIL]%s Spinlock counter mismatch! (%llu != %llu)\n\n", COLOR_RED, COLOR_RESET, val, expected);
    }

    /* Test 4: Mutex Synchronization */
    printf("[%sTEST 4: MUTEX SYNCHRONIZATION (struct mutex)%s]\n", COLOR_YELLOW, COLOR_RESET);
    run_benchmark(fd, LOCKING_IOCTL_INC_MUTEX, "struct mutex", &val, &duration);
    printf(" -> Expected: %llu, Actual: %llu (Elapsed: %.4f s, %.0f ops/sec)\n",
           expected, val, duration, (double)expected / duration);
    if (val == expected) {
        printf("%s[PASS]%s 100%% precision verified! Zero lost increments.\n\n", COLOR_GREEN, COLOR_RESET);
    } else {
        printf("%s[FAIL]%s Mutex counter mismatch! (%llu != %llu)\n\n", COLOR_RED, COLOR_RESET, val, expected);
    }

    close(fd);

    printf("%s=================================================================%s\n", COLOR_BLUE, COLOR_RESET);
    printf("%s       All Concurrency and Locking Test Phases Completed!       %s\n", COLOR_GREEN, COLOR_RESET);
    printf("%s=================================================================%s\n", COLOR_BLUE, COLOR_RESET);

    return 0;
}

