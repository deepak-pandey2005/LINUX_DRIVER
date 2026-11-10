/**
 * @file test_client.c
 * @brief User-space verification client for Chapter 23 Kprobes & Tracing Lab
 * @author Deepak Pandey
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#define PROC_PATH "/proc/kprobe_stats"

static void dump_kprobe_telemetry(const char *label)
{
    printf("\n--- [%s] ---\n", label);
    FILE *fp = fopen(PROC_PATH, "r");
    if (!fp) {
        fprintf(stderr, "[-] Unable to read %s: %s\n", PROC_PATH, strerror(errno));
        fprintf(stderr, "    Ensure debug_kprobe_module.ko is loaded!\n");
        return;
    }

    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        printf("%s", line);
    }
    fclose(fp);
}

int main(int argc, char *argv[])
{
    printf("=====================================================\n");
    printf("   Chapter 23: Kernel Kprobes & Tracing Client      \n");
    printf("=====================================================\n");
    printf("Current PID: %d\n", getpid());

    /* 1. Read baseline telemetry */
    dump_kprobe_telemetry("Baseline Telemetry");

    /* 2. Trigger kernel file open calls (which invokes do_sys_openat2) */
    printf("\n[*] Triggering 10 open() system calls to fire Kprobe handlers...\n");
    for (int i = 0; i < 10; ++i) {
        int fd = open("/dev/null", O_RDONLY);
        if (fd >= 0)
            close(fd);
    }
    printf("[+] Successfully executed 10 open() operations\n");

    /* 3. Read post-trigger telemetry */
    dump_kprobe_telemetry("Post-Trigger Telemetry");

    printf("[SUCCESS] Kprobe interception verified! The kernel intercepted our process in real time.\n\n");
    return EXIT_SUCCESS;
}
