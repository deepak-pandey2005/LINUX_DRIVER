/**
 * @file test_client.c
 * @brief User-space verification client for Chapter 28 Linux PWM Subsystem
 * @author Deepak Pandey
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <errno.h>

#define PWM_CLASS_DIR "/sys/class/pwm"
#define PLATFORM_STATS "/sys/devices/platform/mock_pwm_chip/pwm_stats"

static char g_pwmchip_path[256] = {0};

static int find_pwmchip(void)
{
    DIR *dir = opendir(PWM_CLASS_DIR);
    if (!dir)
        return -1;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strncmp(entry->d_name, "pwmchip", 7) == 0) {
            char uevent_path[512];
            snprintf(uevent_path, sizeof(uevent_path), "%s/%s/device/uevent", PWM_CLASS_DIR, entry->d_name);
            FILE *fp = fopen(uevent_path, "r");
            if (fp) {
                char line[256];
                while (fgets(line, sizeof(line), fp)) {
                    if (strstr(line, "OF_COMPATIBLE_N=0") || strstr(line, "DRIVER=mock_pwm_chip") ||
                        strstr(line, "mock_pwm_chip")) {
                        snprintf(g_pwmchip_path, sizeof(g_pwmchip_path), "%s/%.64s", PWM_CLASS_DIR, entry->d_name);
                        fclose(fp);
                        closedir(dir);
                        return 0;
                    }
                }
                fclose(fp);
            }
        }
    }

    closedir(dir);
    return -1;
}

static int write_sysfs_str(const char *subpath, const char *val)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", g_pwmchip_path, subpath);

    FILE *fp = fopen(path, "w");
    if (!fp)
        return -1;

    fprintf(fp, "%s", val);
    fclose(fp);
    return 0;
}

static int read_sysfs_str(const char *subpath, char *buf, size_t buflen)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", g_pwmchip_path, subpath);

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

static void dump_platform_stats(void)
{
    FILE *fp = fopen(PLATFORM_STATS, "r");
    if (!fp) {
        printf("[-] Unable to read platform stats at %s: %s\n", PLATFORM_STATS, strerror(errno));
        return;
    }

    printf("\n[*] PWM Subsystem Controller Telemetry (%s):\n", PLATFORM_STATS);
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        printf("    %s", line);
    }
    fclose(fp);
}

int main(int argc, char *argv[])
{
    printf("=====================================================\n");
    printf("    Chapter 28: Linux PWM Subsystem Verification     \n");
    printf("=====================================================\n\n");

    if (find_pwmchip() < 0) {
        fprintf(stderr, "[-] mock_pwm_chip not found under %s\n", PWM_CLASS_DIR);
        fprintf(stderr, "    Ensure pwm_driver_module.ko is loaded!\n");
        return EXIT_FAILURE;
    }

    printf("[+] Found PWM Chip controller: %s\n\n", g_pwmchip_path);

    /* 1. Export PWM Channel 0 */
    printf("[*] Exporting PWM channel 0 via %s/export...\n", g_pwmchip_path);
    if (write_sysfs_str("export", "0") < 0) {
        /* Might already be exported */
        printf("    (Note: Channel 0 may already be exported)\n");
    }

    usleep(50000); /* Allow uevent/sysfs creation */

    /* 2. Configure 1 kHz period (1,000,000 ns) */
    printf("[*] Configuring Period to 1,000,000 ns (1 kHz frequency)...\n");
    if (write_sysfs_str("pwm0/period", "1000000") < 0) {
        fprintf(stderr, "[-] Failed to write period (ensure root permissions)\n");
    }

    /* 3. Sweep Duty Cycles */
    const char *duty_cycles[] = { "250000", "500000", "750000" };
    const char *duty_labels[] = { "25%", "50%", "75%" };

    for (int i = 0; i < 3; ++i) {
        printf("[*] Setting Duty Cycle to %s ns (%s power)...\n", duty_cycles[i], duty_labels[i]);
        write_sysfs_str("pwm0/duty_cycle", duty_cycles[i]);
        usleep(30000);
    }

    /* 4. Enable Output */
    printf("[*] Enabling PWM Channel 0...\n");
    write_sysfs_str("pwm0/enable", "1");

    char period_buf[32] = {0};
    char duty_buf[32] = {0};
    char enable_buf[32] = {0};

    read_sysfs_str("pwm0/period", period_buf, sizeof(period_buf));
    read_sysfs_str("pwm0/duty_cycle", duty_buf, sizeof(duty_buf));
    read_sysfs_str("pwm0/enable", enable_buf, sizeof(enable_buf));

    printf("    [ACTIVE PWM STATE]: Period=%s ns, Duty=%s ns, Enabled=%s\n",
           period_buf, duty_buf, enable_buf);

    /* 5. Disable and Unexport */
    printf("\n[*] Disabling PWM Channel 0...\n");
    write_sysfs_str("pwm0/enable", "0");
    write_sysfs_str("unexport", "0");

    /* 6. Dump telemetry from driver */
    dump_platform_stats();

    printf("\n[SUCCESS] Chapter 28 PWM Subsystem verification complete!\n");
    return EXIT_SUCCESS;
}
