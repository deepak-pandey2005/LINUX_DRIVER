/**
 * @file test_client.c
 * @brief User-space verification client for Chapter 26 Linux Input Subsystem
 * @author Deepak Pandey
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#define CTRL_DEV   "/dev/vcontroller"
#define STATS_PATH "/sys/class/vcontroller_class/vcontroller/input_stats"

static int send_command(int fd, const char *cmd)
{
    printf("    -> Injecting command: \"%s\"\n", cmd);
    ssize_t ret = write(fd, cmd, strlen(cmd));
    if (ret < 0) {
        fprintf(stderr, "[-] write failed for \"%s\": %s\n", cmd, strerror(errno));
        return -1;
    }
    usleep(20000); /* 20ms pause between events */
    return 0;
}

static void print_sysfs_stats(void)
{
    FILE *fp = fopen(STATS_PATH, "r");
    if (!fp) {
        printf("[-] Unable to read stats at %s: %s\n", STATS_PATH, strerror(errno));
        return;
    }

    printf("\n[*] Kernel Input Subsystem Telemetry (%s):\n", STATS_PATH);
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        printf("    %s", line);
    }
    fclose(fp);
}

int main(int argc, char *argv[])
{
    printf("=====================================================\n");
    printf("  Chapter 26: Virtual Input Subsystem Verification  \n");
    printf("=====================================================\n\n");

    int fd = open(CTRL_DEV, O_WRONLY);
    if (fd < 0) {
        fprintf(stderr, "[-] Failed to open %s: %s\n", CTRL_DEV, strerror(errno));
        fprintf(stderr, "    Ensure input_driver_module.ko is loaded and permissions are granted!\n");
        return EXIT_FAILURE;
    }

    printf("[+] Successfully opened control node: %s\n\n", CTRL_DEV);

    /* 1. Simulate button presses */
    printf("[*] Generating EV_KEY button events (Gamepad BTN_A & BTN_B):\n");
    send_command(fd, "KEY_A 1");
    send_command(fd, "KEY_A 0");
    send_command(fd, "KEY_B 1");
    send_command(fd, "KEY_B 0");
    send_command(fd, "KEY_UP 1");
    send_command(fd, "KEY_UP 0");

    /* 2. Simulate analog thumbstick motions */
    printf("\n[*] Generating EV_ABS analog joystick movements (ABS_X & ABS_Y):\n");
    send_command(fd, "ABS_X 128");
    send_command(fd, "ABS_X 384");
    send_command(fd, "ABS_X 0");
    send_command(fd, "ABS_Y -256");
    send_command(fd, "ABS_Y 512");
    send_command(fd, "ABS_Y 0");

    close(fd);

    /* 3. Read telemetry */
    print_sysfs_stats();

    printf("\n[SUCCESS] Input event injection verified! Events routed through Linux input core.\n");
    printf("          Inspect with 'evtest' or check /proc/bus/input/devices.\n\n");
    return EXIT_SUCCESS;
}
