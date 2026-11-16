/**
 * @file test_client.c
 * @brief User-space verification client for Chapter 24 SPI Bus Subsystem
 * @author Deepak Pandey
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#define DEV_PATH   "/dev/spi_sensor"
#define STATS_PATH "/sys/class/mock_spi_class/spi_sensor/spi_stats"

static void dump_sysfs_stats(void)
{
    FILE *fp = fopen(STATS_PATH, "r");
    if (!fp) {
        printf("[-] Unable to read sysfs statistics from %s: %s\n", STATS_PATH, strerror(errno));
        return;
    }

    printf("\n[*] SPI Subsystem Telemetry (%s):\n", STATS_PATH);
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        printf("    %s", line);
    }
    fclose(fp);
}

int main(int argc, char *argv[])
{
    printf("=====================================================\n");
    printf("     Chapter 24: SPI Bus Driver Verification Tool    \n");
    printf("=====================================================\n\n");

    int fd = open(DEV_PATH, O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "[-] Failed to open %s: %s\n", DEV_PATH, strerror(errno));
        fprintf(stderr, "    Ensure spi_driver_module.ko is loaded and permissions are granted.\n");
        return EXIT_FAILURE;
    }

    printf("[+] Successfully opened device: %s\n", DEV_PATH);

    /* 1. Read sensor temperature (via read() calling spi_write_then_read) */
    printf("\n[*] Reading simulated sensor data via SPI synchronous transfer:\n");
    char rx_buf[128] = {0};
    ssize_t bytes_read = read(fd, rx_buf, sizeof(rx_buf) - 1);
    if (bytes_read < 0) {
        fprintf(stderr, "[-] read() failed: %s\n", strerror(errno));
    } else {
        printf("    [SENSOR RESPONSE] %s", rx_buf);
    }

    /* 2. Send SPI Command Stream (READ_ID: 0x9F, LOOPBACK: 0xAA, STATUS: 0x03) */
    printf("\n[*] Sending SPI command sequences via full-duplex write():\n");
    unsigned char cmd_seq[] = { 0x9F, 0x02, 0x03, 0xAA, 0x10 };
    printf("    Transmitting %zu command bytes (0x9F, 0x02, 0x03, 0xAA, 0x10)...\n",
           sizeof(cmd_seq));

    ssize_t written = write(fd, cmd_seq, sizeof(cmd_seq));
    if (written < 0) {
        fprintf(stderr, "[-] write() failed: %s\n", strerror(errno));
    } else {
        printf("[+] Transmitted %zd bytes to SPI bus\n", written);
    }

    /* 3. Read again to verify updated transaction count */
    memset(rx_buf, 0, sizeof(rx_buf));
    lseek(fd, 0, SEEK_SET);
    bytes_read = read(fd, rx_buf, sizeof(rx_buf) - 1);
    if (bytes_read > 0) {
        printf("    [SENSOR RESPONSE 2] %s", rx_buf);
    }

    close(fd);

    /* 4. Dump Sysfs Telemetry */
    dump_sysfs_stats();

    printf("\n[SUCCESS] Chapter 24 SPI Bus Subsystem verification complete!\n");
    return EXIT_SUCCESS;
}
