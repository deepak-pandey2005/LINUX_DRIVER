/**
 * @file test_app.c
 * @brief User-space test client verifying read/write operations on cdev_buffer_dev.
 * @author Deepak Pandey
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#define DEVICE_PATH "/dev/cdev_buffer_dev"
#define TEST_STRING "Linux Driver Development: Round-trip User-to-Kernel buffer test!"

int main(void)
{
    int fd;
    ssize_t bytes_written, bytes_read;
    char read_buffer[256];

    printf("====================================================\n");
    printf("User-Space Test Client: Character Device I/O\n");
    printf("====================================================\n");

    // 1. Open the device node
    printf("[1] Opening device: %s...\n", DEVICE_PATH);
    fd = open(DEVICE_PATH, O_RDWR);
    if (fd < 0) {
        perror("[-] Failed to open device");
        printf("    Make sure the module is loaded and node created via 'make create_node'\n");
        return EXIT_FAILURE;
    }
    printf("[+] Successfully opened device (fd = %d)\n\n", fd);

    // 2. Write data to the kernel driver
    printf("[2] Writing to kernel driver: \"%s\" (%zu bytes)\n",
           TEST_STRING, strlen(TEST_STRING));
    bytes_written = write(fd, TEST_STRING, strlen(TEST_STRING));
    if (bytes_written < 0) {
        perror("[-] Failed to write to device");
        close(fd);
        return EXIT_FAILURE;
    }
    printf("[+] Successfully wrote %zd bytes to driver!\n\n", bytes_written);

    // 3. Reset file offset using lseek or close & reopen
    close(fd);
    fd = open(DEVICE_PATH, O_RDONLY);
    if (fd < 0) {
        perror("[-] Failed to reopen device for reading");
        return EXIT_FAILURE;
    }

    // 4. Read back the data stored in kernel memory
    memset(read_buffer, 0, sizeof(read_buffer));
    printf("[3] Reading back from kernel driver...\n");
    bytes_read = read(fd, read_buffer, sizeof(read_buffer) - 1);
    if (bytes_read < 0) {
        perror("[-] Failed to read from device");
        close(fd);
        return EXIT_FAILURE;
    }
    printf("[+] Successfully read %zd bytes!\n", bytes_read);
    printf("    Data received from kernel: \"%s\"\n\n", read_buffer);

    // 5. Verification
    if (strcmp(TEST_STRING, read_buffer) == 0) {
        printf("[SUCCESS] Read/Write test PASSED! Kernel buffer matched user data.\n");
    } else {
        printf("[FAILURE] Data mismatch between written and read content!\n");
    }

    // 6. Close handle
    close(fd);
    printf("[4] Device closed.\n");
    printf("====================================================\n");

    return EXIT_SUCCESS;
}
