/**
 * @file test_client.c
 * @brief User-space verification client for Chapter 21 Virtual Network Driver
 * @author Deepak Pandey
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <net/if.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <linux/if_packet.h>
#include <net/ethernet.h>

#define IFACE_NAME "vnet0"

static int read_sysfs_str(const char *subpath, char *buf, size_t buflen)
{
    char path[256];
    snprintf(path, sizeof(path), "/sys/class/net/%s/%s", IFACE_NAME, subpath);
    FILE *fp = fopen(path, "r");
    if (!fp)
        return -1;
    if (!fgets(buf, buflen, fp)) {
        fclose(fp);
        return -1;
    }
    fclose(fp);
    buf[strcspn(buf, "\r\n")] = '\0';
    return 0;
}

static unsigned long long read_sysfs_stat(const char *stat_name)
{
    char buf[64];
    char subpath[128];
    snprintf(subpath, sizeof(subpath), "statistics/%s", stat_name);
    if (read_sysfs_str(subpath, buf, sizeof(buf)) < 0)
        return 0;
    return strtoull(buf, NULL, 10);
}

int main(int argc, char *argv[])
{
    printf("=====================================================\n");
    printf("  Chapter 21: Virtual Network Driver Inspection Tool\n");
    printf("=====================================================\n\n");

    unsigned int ifindex = if_nametoindex(IFACE_NAME);
    if (ifindex == 0) {
        fprintf(stderr, "[-] Interface '%s' not found! Is virtual_net_module loaded?\n", IFACE_NAME);
        fprintf(stderr, "    Load module with: sudo insmod virtual_net_module.ko\n");
        return EXIT_FAILURE;
    }

    printf("[+] Found interface '%s' (ifindex: %u)\n", IFACE_NAME, ifindex);

    char addr[64] = {0};
    char operstate[64] = {0};
    char mtu[64] = {0};

    if (read_sysfs_str("address", addr, sizeof(addr)) == 0) {
        printf("    MAC Address: %s\n", addr);
    }
    if (read_sysfs_str("operstate", operstate, sizeof(operstate)) == 0) {
        printf("    Operational State: %s\n", operstate);
    }
    if (read_sysfs_str("mtu", mtu, sizeof(mtu)) == 0) {
        printf("    MTU: %s bytes\n", mtu);
    }

    printf("\n[*] Reading Network Interface Telemetry Statistics:\n");
    printf("    TX Packets: %llu\n", read_sysfs_stat("tx_packets"));
    printf("    TX Bytes:   %llu bytes\n", read_sysfs_stat("tx_bytes"));
    printf("    RX Packets: %llu\n", read_sysfs_stat("rx_packets"));
    printf("    RX Bytes:   %llu bytes\n", read_sysfs_stat("rx_bytes"));
    printf("    RX Dropped: %llu\n", read_sysfs_stat("rx_dropped"));

    printf("\n[+] Verification Suggestions:\n");
    printf("    1. Bring interface UP:\n");
    printf("       $ sudo ip link set %s up\n", IFACE_NAME);
    printf("    2. Assign IP address:\n");
    printf("       $ sudo ip addr add 192.168.42.10/24 dev %s\n", IFACE_NAME);
    printf("    3. Transmit test ping packet (virtual loopback will echo back):\n");
    printf("       $ ping -c 3 192.168.42.10\n");
    printf("    4. Re-run this tool to observe incrementing packet and byte counters.\n\n");

    return EXIT_SUCCESS;
}

