/**
 * @file test_client.c
 * @brief User-space verification client for Chapter 27 Watchdog Subsystem
 * @author Deepak Pandey
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/watchdog.h>

#define WATCHDOG_DEV "/dev/watchdog"

int main(int argc, char *argv[])
{
    printf("=====================================================\n");
    printf("     Chapter 27: Watchdog Subsystem Verification    \n");
    printf("=====================================================\n\n");

    int fd = open(WATCHDOG_DEV, O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "[-] Failed to open %s: %s\n", WATCHDOG_DEV, strerror(errno));
        fprintf(stderr, "    Ensure watchdog_driver_module.ko is loaded and permissions are granted!\n");
        return EXIT_FAILURE;
    }

    printf("[+] Successfully opened %s (Watchdog is now ARMED)\n", WATCHDOG_DEV);

    /* 1. Query Watchdog Identity */
    struct watchdog_info info;
    if (ioctl(fd, WDIOC_GETSUPPORT, &info) == 0) {
        printf("    Device Identity: %s\n", info.identity);
        printf("    Driver Version:  %u\n", info.firmware_version);
        printf("    Options Bitmask: 0x%08x\n", info.options);
    }

    /* 2. Query Current Timeout */
    int timeout = 0;
    if (ioctl(fd, WDIOC_GETTIMEOUT, &timeout) == 0) {
        printf("    Initial Timeout: %d seconds\n", timeout);
    }

    /* 3. Query Time Remaining */
    int timeleft = 0;
    if (ioctl(fd, WDIOC_GETTIMELEFT, &timeleft) == 0) {
        printf("    Time Left:       %d seconds\n\n", timeleft);
    }

    /* 4. Update Timeout to 15 seconds */
    int new_timeout = 15;
    printf("[*] Adjusting timeout to %d seconds via WDIOC_SETTIMEOUT...\n", new_timeout);
    if (ioctl(fd, WDIOC_SETTIMEOUT, &new_timeout) == 0) {
        printf("[+] Updated timeout confirmed: %d seconds\n\n", new_timeout);
    }

    /* 5. Simulate periodic heartbeat (petting the watchdog) */
    printf("[*] Simulating heartbeat loop (petting watchdog every 1 second)...\n");
    for (int i = 1; i <= 3; ++i) {
        sleep(1);
        if (ioctl(fd, WDIOC_GETTIMELEFT, &timeleft) == 0) {
            printf("    [Tick %d] Time left before ping: %d sec\n", i, timeleft);
        }

        /* Ping via ioctl */
        if (ioctl(fd, WDIOC_KEEPALIVE, 0) == 0) {
            printf("    [Tick %d] Heartbeat ping sent successfully!\n", i);
        }

        if (ioctl(fd, WDIOC_GETTIMELEFT, &timeleft) == 0) {
            printf("    [Tick %d] Time left after ping:  %d sec\n\n", i, timeleft);
        }
    }

    /* 6. Clean Shutdown via Magic Close ('V') */
    printf("[*] Sending Magic Character 'V' to permit safe shutdown without triggering reboot...\n");
    if (write(fd, "V", 1) < 0) {
        fprintf(stderr, "[-] Failed to write Magic Character: %s\n", strerror(errno));
    }

    close(fd);
    printf("[+] Watchdog closed safely.\n\n");

    printf("[SUCCESS] Chapter 27 Watchdog Subsystem verification complete!\n");
    return EXIT_SUCCESS;
}
