/**
 * @file test_client.c
 * @brief User-space client testing automatically created /dev/auto_char_dev node.
 * @author Deepak Pandey
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#define DEVICE_PATH "/dev/auto_char_dev"
#define PAYLOAD "udev automatic device creation works flawlessly!"

int main(void)
{
    struct stat st;
    int fd;
    ssize_t written, read_bytes;
    char buffer[128];

    printf("====================================================\n");
    printf("User-Space Client: Testing Auto-Created Device Node\n");
    printf("====================================================\n");

    // 1. Verify existence of device node
    if (stat(DEVICE_PATH, &st) != 0) {
        perror("[-] stat failed: /dev/auto_char_dev does NOT exist");
        printf("    Is udev_device_module.ko loaded? Run 'make load'\n");
        return EXIT_FAILURE;
    }

    printf("[+] /dev/auto_char_dev exists in filesystem!\n");
    printf("    Permissions: %o (Octal)\n", st.st_mode & 0777);
    printf("    Device Major: %d, Minor: %d\n\n",
           (int)(st.st_rdev >> 8), (int)(st.st_rdev & 0xff));

    // 2. Open without requiring sudo
    printf("[1] Opening %s (unprivileged access)...\n", DEVICE_PATH);
    fd = open(DEVICE_PATH, O_RDWR);
    if (fd < 0) {
        perror("[-] open failed");
        return EXIT_FAILURE;
    }
    printf("[+] Successfully opened device (fd = %d)\n\n", fd);

    // 3. Write data
    printf("[2] Writing test payload: \"%s\"\n", PAYLOAD);
    written = write(fd, PAYLOAD, strlen(PAYLOAD));
    if (written < 0) {
        perror("[-] write failed");
        close(fd);
        return EXIT_FAILURE;
    }
    printf("[+] Wrote %zd bytes\n\n", written);

    // 4. Reopen and read
    close(fd);
    fd = open(DEVICE_PATH, O_RDONLY);
    if (fd < 0) {
        perror("[-] reopen failed");
        return EXIT_FAILURE;
    }

    memset(buffer, 0, sizeof(buffer));
    read_bytes = read(fd, buffer, sizeof(buffer) - 1);
    if (read_bytes < 0) {
        perror("[-] read failed");
        close(fd);
        return EXIT_FAILURE;
    }

    printf("[3] Read back from kernel driver: \"%s\" (%zd bytes)\n\n", buffer, read_bytes);

    if (strcmp(PAYLOAD, buffer) == 0) {
        printf("[SUCCESS] Verified round-trip I/O on auto-created node!\n");
    } else {
        printf("[FAILURE] Data mismatch!\n");
    }

    close(fd);
    printf("====================================================\n");
    return EXIT_SUCCESS;
}
