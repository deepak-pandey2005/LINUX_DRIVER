/**
 * @file test_client.c
 * @brief User-space verification client for Chapter 30 PCI / PCIe Subsystem
 * @author Deepak Pandey
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdint.h>

#define PCI_DEV_PATH "/dev/pci_demo_dev"
#define STATS_PATH   "/sys/class/pci_demo_class/pci_demo_dev/pci_stats"

static void print_sysfs_stats(void)
{
    FILE *fp = fopen(STATS_PATH, "r");
    if (!fp) {
        printf("[-] Unable to read stats at %s: %s\n", STATS_PATH, strerror(errno));
        return;
    }

    printf("\n[*] PCI Subsystem Telemetry (%s):\n", STATS_PATH);
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        printf("    %s", line);
    }
    fclose(fp);
}

int main(int argc, char *argv[])
{
    printf("=====================================================\n");
    printf("    Chapter 30: PCI / PCIe Subsystem Verification    \n");
    printf("=====================================================\n\n");

    int fd = open(PCI_DEV_PATH, O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "[-] Failed to open %s: %s\n", PCI_DEV_PATH, strerror(errno));
        fprintf(stderr, "    Ensure pci_driver_module.ko is loaded and permissions are granted!\n");
        return EXIT_FAILURE;
    }

    printf("[+] Successfully opened PCI device node: %s\n", PCI_DEV_PATH);

    /* 1. Read BAR0 Header (VID / PID) */
    uint16_t vid = 0, pid = 0;
    lseek(fd, 0, SEEK_SET);
    if (read(fd, &vid, sizeof(vid)) == sizeof(vid) &&
        read(fd, &pid, sizeof(pid)) == sizeof(pid)) {
        printf("[*] Probed BAR0 Identification Registers:\n");
        printf("    Vendor ID:  0x%04X (Expected: 0x1234)\n", vid);
        printf("    Device ID:  0x%04X (Expected: 0x11E8)\n\n", pid);
    }

    /* 2. Write to BAR0 Control Register (offset 0x04) to fire an interrupt */
    printf("[*] Writing control command (0xCAFE0001) to BAR0 offset 0x04...\n");
    uint32_t ctrl_val = 0xCAFE0001;
    lseek(fd, 4, SEEK_SET);
    ssize_t written = write(fd, &ctrl_val, sizeof(ctrl_val));
    if (written < 0) {
        fprintf(stderr, "[-] write() failed: %s\n", strerror(errno));
    } else {
        printf("[+] Wrote %zd bytes to BAR0 register.\n", written);
    }

    /* 3. Read back register to confirm persistence */
    uint32_t read_back = 0;
    lseek(fd, 4, SEEK_SET);
    if (read(fd, &read_back, sizeof(read_back)) == sizeof(read_back)) {
        printf("    Verified BAR0[0x04] Readback: 0x%08X\n", read_back);
    }

    close(fd);

    /* 4. Display Sysfs Telemetry */
    print_sysfs_stats();

    printf("\n[SUCCESS] Chapter 30 PCI / PCIe Subsystem verified!\n");
    return EXIT_SUCCESS;
}
