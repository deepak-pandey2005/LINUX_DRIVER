/**
 * @file test_client.c
 * @brief User-space verification client for Chapter 22 USB Driver Subsystem
 * @author Deepak Pandey
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#define DEV_PATH   "/dev/usb_demo_dev"
#define STATS_PATH "/sys/class/usb_demo_class/usb_demo_dev/usb_stats"

static void print_sysfs_stats(void)
{
    FILE *fp = fopen(STATS_PATH, "r");
    if (!fp) {
        printf("[-] Unable to read sysfs statistics at %s: %s\n", STATS_PATH, strerror(errno));
        return;
    }

    printf("\n[*] Kernel USB Driver Telemetry (/sys/class/usb_demo_class/usb_demo_dev/usb_stats):\n");
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        printf("    %s", line);
    }
    fclose(fp);
}

int main(int argc, char *argv[])
{
    printf("=====================================================\n");
    printf("     Chapter 22: USB Driver Verification Client     \n");
    printf("=====================================================\n\n");

    int fd = open(DEV_PATH, O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "[-] Failed to open %s: %s\n", DEV_PATH, strerror(errno));
        fprintf(stderr, "    Ensure usb_driver_module.ko is loaded and permissions are granted.\n");
        return EXIT_FAILURE;
    }

    printf("[+] Successfully opened device: %s\n", DEV_PATH);

    const char *test_message = "Hello USB Subsystem! Testing asynchronous bulk URB loopback pipeline.";
    size_t msg_len = strlen(test_message);

    printf("[*] Transmitting test payload (%zu bytes): \"%s\"\n", msg_len, test_message);
    ssize_t written = write(fd, test_message, msg_len);
    if (written < 0) {
        fprintf(stderr, "[-] write() failed: %s\n", strerror(errno));
        close(fd);
        return EXIT_FAILURE;
    }
    printf("[+] Successfully transmitted %zd bytes to USB driver\n", written);

    char rx_buf[256] = {0};
    printf("[*] Reading received data from USB driver...\n");
    ssize_t read_bytes = read(fd, rx_buf, sizeof(rx_buf) - 1);
    if (read_bytes < 0) {
        fprintf(stderr, "[-] read() failed: %s\n", strerror(errno));
        close(fd);
        return EXIT_FAILURE;
    }
    printf("[+] Received %zd bytes: \"%s\"\n", read_bytes, rx_buf);

    if (strncmp(test_message, rx_buf, msg_len) == 0) {
        printf("[SUCCESS] Payload integrity verified! Sent and received data match exactly.\n");
    } else {
        printf("[WARNING] Payload mismatch!\n");
    }

    close(fd);

    /* Read and display telemetry from sysfs */
    print_sysfs_stats();

    return EXIT_SUCCESS;
}

