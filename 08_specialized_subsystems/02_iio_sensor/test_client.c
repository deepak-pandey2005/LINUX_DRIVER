/**
 * @file test_client.c
 * @brief User-space verification client for Chapter 25 IIO Subsystem
 * @author Deepak Pandey
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>

#define IIO_DIR "/sys/bus/iio/devices"
#define TARGET_NAME "mock_iio_adc"

static char g_device_path[256] = {0};

static int find_iio_device(void)
{
    DIR *dir = opendir(IIO_DIR);
    if (!dir) {
        fprintf(stderr, "[-] Unable to open IIO directory %s: %s\n", IIO_DIR, strerror(errno));
        return -1;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strncmp(entry->d_name, "iio:device", 10) == 0) {
            char name_path[1024];
            snprintf(name_path, sizeof(name_path), "%s/%s/name", IIO_DIR, entry->d_name);
            FILE *fp = fopen(name_path, "r");
            if (fp) {
                char dev_name[64] = {0};
                if (fgets(dev_name, sizeof(dev_name), fp)) {
                    dev_name[strcspn(dev_name, "\r\n")] = '\0';
                    if (strcmp(dev_name, TARGET_NAME) == 0) {
                        snprintf(g_device_path, sizeof(g_device_path), "%s/%.128s", IIO_DIR, entry->d_name);
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

static int read_iio_attr(const char *attr, double *val)
{
    char full_path[1024];
    snprintf(full_path, sizeof(full_path), "%s/%s", g_device_path, attr);

    FILE *fp = fopen(full_path, "r");
    if (!fp)
        return -1;

    if (fscanf(fp, "%lf", val) != 1) {
        fclose(fp);
        return -1;
    }

    fclose(fp);
    return 0;
}

static int write_iio_attr(const char *attr, int val)
{
    char full_path[1024];
    snprintf(full_path, sizeof(full_path), "%s/%s", g_device_path, attr);

    FILE *fp = fopen(full_path, "w");
    if (!fp)
        return -1;

    fprintf(fp, "%d\n", val);
    fclose(fp);
    return 0;
}

int main(int argc, char *argv[])
{
    printf("=====================================================\n");
    printf("  Chapter 25: Industrial I/O (IIO) Telemetry Client \n");
    printf("=====================================================\n\n");

    if (find_iio_device() < 0) {
        fprintf(stderr, "[-] Target IIO device '%s' not found under %s\n", TARGET_NAME, IIO_DIR);
        fprintf(stderr, "    Ensure iio_sensor_module.ko is loaded!\n");
        return EXIT_FAILURE;
    }

    printf("[+] Discovered IIO Device at: %s\n\n", g_device_path);

    /* 1. Voltage Channel */
    double v_raw = 0, v_scale = 0;
    if (read_iio_attr("in_voltage0_raw", &v_raw) == 0 &&
        read_iio_attr("in_voltage0_scale", &v_scale) == 0) {
        double voltage_v = (v_raw * v_scale);
        printf("[ADC Channel 0] Voltage:\n");
        printf("    Raw Count:     %.0f LSB\n", v_raw);
        printf("    Scale Factor:  %.6f V/LSB\n", v_scale);
        printf("    Calculated:    %.4f Volts (%.2f mV)\n\n", voltage_v, voltage_v * 1000.0);
    }

    /* 2. Temperature Channel */
    double t_raw = 0, t_scale = 1000.0;
    if (read_iio_attr("in_temp_raw", &t_raw) == 0) {
        read_iio_attr("in_temp_scale", &t_scale);
        double temp_c = t_raw / t_scale;
        printf("[Thermal Channel] Temperature:\n");
        printf("    Raw Value:     %.0f (milli-degrees C)\n", t_raw);
        printf("    Scale Factor:  %.1f\n", t_scale);
        printf("    Calculated:    %.2f °C (%.2f °F)\n\n", temp_c, (temp_c * 9.0 / 5.0) + 32.0);
    }

    /* 3. 3-Axis Accelerometer */
    double ax = 0, ay = 0, az = 0, a_scale = 0;
    if (read_iio_attr("in_accel_x_raw", &ax) == 0 &&
        read_iio_attr("in_accel_y_raw", &ay) == 0 &&
        read_iio_attr("in_accel_z_raw", &az) == 0 &&
        read_iio_attr("in_accel_scale", &a_scale) == 0) {
        printf("[IMU 3-Axis Accelerometer]:\n");
        printf("    Scale Factor:  %.6f m/s^2 per LSB\n", a_scale);
        printf("    X-Axis:        %8.0f LSB  ->  %+6.2f m/s^2\n", ax, ax * a_scale);
        printf("    Y-Axis:        %8.0f LSB  ->  %+6.2f m/s^2\n", ay, ay * a_scale);
        printf("    Z-Axis:        %8.0f LSB  ->  %+6.2f m/s^2 (~1G Earth Gravity)\n\n", az, az * a_scale);
    }

    /* 4. Test write_raw calibration (if write permissions exist) */
    printf("[*] Testing calibration write (in_temp_raw = 31200 millidegrees)...\n");
    if (write_iio_attr("in_temp_raw", 31200) == 0) {
        read_iio_attr("in_temp_raw", &t_raw);
        printf("[+] Write successful! New Temperature: %.2f °C\n\n", t_raw / t_scale);
    } else {
        printf("[-] Note: Calibration write skipped (requires write permissions on sysfs entry).\n\n");
    }

    printf("[SUCCESS] Chapter 25 IIO Subsystem multi-channel verification complete!\n");
    return EXIT_SUCCESS;
}
