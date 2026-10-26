/**
 * @file test_client.c
 * @brief User-space test suite verifying circular wrap-around, buffer limits, and fault injection.
 * @author Deepak Pandey
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#define DEVICE_PATH "/dev/ring_buffer_dev"
#define RING_CAPACITY 1024

static int run_basic_test(int fd)
{
    const char msg[] = "Test 1: Hello from User Space through the Ring Buffer!";
    char rx_buf[128];
    ssize_t written, read_bytes;

    printf("\n--- [TEST 1: Basic Write and Read] ---\n");
    written = write(fd, msg, strlen(msg));
    if (written < 0) {
        perror("[-] write failed");
        return -1;
    }
    printf("[+] Wrote %zd bytes: \"%s\"\n", written, msg);

    memset(rx_buf, 0, sizeof(rx_buf));
    read_bytes = read(fd, rx_buf, sizeof(rx_buf) - 1);
    if (read_bytes < 0) {
        perror("[-] read failed");
        return -1;
    }
    printf("[+] Read back %zd bytes: \"%s\"\n", read_bytes, rx_buf);

    if (strcmp(msg, rx_buf) == 0) {
        printf("[PASS] Basic test passed!\n");
        return 0;
    } else {
        printf("[FAIL] Data mismatch!\n");
        return -1;
    }
}

static int run_wraparound_test(int fd)
{
    char send_buf[600];
    char recv_buf[600];
    char dummy_buf[700];
    ssize_t ret;
    int i;

    printf("\n--- [TEST 2: Circular Wrap-Around Boundary Test] ---\n");
    // Step A: Write 700 dummy bytes and read them out to shift head/tail to index 700
    memset(dummy_buf, 'A', sizeof(dummy_buf));
    ret = write(fd, dummy_buf, sizeof(dummy_buf));
    if (ret != sizeof(dummy_buf)) {
        printf("[-] Setup write failed\n");
        return -1;
    }
    ret = read(fd, dummy_buf, sizeof(dummy_buf));
    if (ret != sizeof(dummy_buf)) {
        printf("[-] Setup read failed\n");
        return -1;
    }
    printf("[+] Position shifted: head and tail are now at index 700\n");

    // Step B: Write 600 bytes. This MUST wrap: 700..1024 (324 bytes) and 0..276 (276 bytes)!
    for (i = 0; i < sizeof(send_buf); i++) {
        send_buf[i] = (char)('a' + (i % 26));
    }

    printf("[+] Writing 600 bytes across array boundary (324 at tail, 276 at head)...\n");
    ret = write(fd, send_buf, sizeof(send_buf));
    if (ret != sizeof(send_buf)) {
        perror("[-] Wrap-around write failed");
        return -1;
    }
    printf("[+] Successfully wrote %zd bytes across wrap boundary!\n", ret);

    // Step C: Read back all 600 bytes across wrap boundary
    memset(recv_buf, 0, sizeof(recv_buf));
    ret = read(fd, recv_buf, sizeof(recv_buf));
    if (ret != sizeof(recv_buf)) {
        perror("[-] Wrap-around read failed");
        return -1;
    }
    printf("[+] Successfully read %zd bytes across wrap boundary!\n", ret);

    // Step D: Byte-for-byte verification
    if (memcmp(send_buf, recv_buf, sizeof(send_buf)) == 0) {
        printf("[PASS] Wrap-around test PASSED! 100%% byte fidelity.\n");
        return 0;
    } else {
        printf("[FAIL] Data corruption during wrap-around!\n");
        return -1;
    }
}

static int run_capacity_test(int fd)
{
    char full_buf[RING_CAPACITY];
    char extra_buf[50];
    char drain_buf[RING_CAPACITY];
    ssize_t ret;

    printf("\n--- [TEST 3: Buffer Capacity Limit & ENOSPC Handling] ---\n");
    memset(full_buf, 'X', sizeof(full_buf));

    // Fill buffer completely
    ret = write(fd, full_buf, sizeof(full_buf));
    printf("[+] Filled buffer with %zd bytes (100%% full)\n", ret);

    // Attempt to write beyond capacity
    printf("[+] Attempting overflow write with 50 extra bytes...\n");
    errno = 0;
    ret = write(fd, extra_buf, sizeof(extra_buf));
    if (ret < 0 && errno == ENOSPC) {
        printf("[PASS] Overflow correctly rejected with -ENOSPC (No space left on device)!\n");
    } else {
        printf("[FAIL] Expected ENOSPC, got ret=%zd, errno=%d\n", ret, errno);
        return -1;
    }

    // Drain buffer
    ret = read(fd, drain_buf, sizeof(drain_buf));
    printf("[+] Drained %zd bytes from buffer\n", ret);
    return 0;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnonnull"
static int run_fault_injection_test(int fd)
{
    ssize_t ret;
    void *bad_ptr = NULL;

    printf("\n--- [TEST 4: Fault Injection & Kernel Pointer Validation] ---\n");
    printf("[+] Injecting NULL user-space pointer into write()...\n");
    errno = 0;
    ret = write(fd, bad_ptr, 64);
    if (ret < 0 && errno == EFAULT) {
        printf("[PASS] Driver safely caught NULL pointer and returned -EFAULT without crashing!\n");
    } else {
        printf("[FAIL] Expected EFAULT, got ret=%zd, errno=%d\n", ret, errno);
        return -1;
    }

    printf("[+] Injecting NULL user-space pointer into read()...\n");
    errno = 0;
    ret = read(fd, bad_ptr, 64);
    if (ret < 0 && errno == EFAULT) {
        printf("[PASS] Driver safely caught NULL pointer in read() and returned -EFAULT!\n");
    } else {
        printf("[FAIL] Expected EFAULT, got ret=%zd, errno=%d\n", ret, errno);
        return -1;
    }

    return 0;
}
#pragma GCC diagnostic pop

int main(void)
{
    int fd;
    int failures = 0;

    printf("====================================================\n");
    printf("   Circular Ring Buffer & Memory Safety Test Suite   \n");
    printf("====================================================\n");

    fd = open(DEVICE_PATH, O_RDWR);
    if (fd < 0) {
        perror("[-] Failed to open " DEVICE_PATH);
        printf("    Make sure module is loaded via 'make load'\n");
        return EXIT_FAILURE;
    }

    if (run_basic_test(fd) != 0) failures++;
    if (run_wraparound_test(fd) != 0) failures++;
    if (run_capacity_test(fd) != 0) failures++;
    if (run_fault_injection_test(fd) != 0) failures++;

    close(fd);

    printf("\n====================================================\n");
    if (failures == 0) {
        printf(" [ALL TESTS PASSED] Ring buffer is 100%% robust!\n");
    } else {
        printf(" [WARNING] %d test(s) failed!\n", failures);
    }
    printf("====================================================\n");

    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
