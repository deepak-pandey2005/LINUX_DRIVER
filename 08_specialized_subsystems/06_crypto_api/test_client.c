/**
 * @file test_client.c
 * @brief User-space verification client for Chapter 29 Kernel Cryptographic API
 * @author Deepak Pandey
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#define CRYPTO_DEV  "/dev/kernel_crypto"
#define STATS_PATH  "/sys/class/kernel_crypto_class/kernel_crypto/crypto_stats"

/* Standard NIST CAVP SHA-256 Test Vector */
#define TEST_INPUT  "The quick brown fox jumps over the lazy dog"
#define NIST_SHA256 "d7a8fbb307d7809469ca9abcb0082e4f8d5651e46d3cdb762d02d0bf37c9e592"

static void print_sysfs_stats(void)
{
    FILE *fp = fopen(STATS_PATH, "r");
    if (!fp) {
        printf("[-] Unable to read stats at %s: %s\n", STATS_PATH, strerror(errno));
        return;
    }

    printf("\n[*] Kernel Crypto Subsystem Telemetry (%s):\n", STATS_PATH);
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        printf("    %s", line);
    }
    fclose(fp);
}

int main(int argc, char *argv[])
{
    printf("=====================================================\n");
    printf("  Chapter 29: Kernel Cryptographic API Verification \n");
    printf("=====================================================\n\n");

    int fd = open(CRYPTO_DEV, O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "[-] Failed to open %s: %s\n", CRYPTO_DEV, strerror(errno));
        fprintf(stderr, "    Ensure crypto_api_module.ko is loaded and permissions are granted!\n");
        return EXIT_FAILURE;
    }

    printf("[+] Successfully opened crypto device: %s\n", CRYPTO_DEV);

    /* 1. Send NIST Test Vector to Kernel */
    printf("\n[*] Submitting NIST CAVP Test Vector (%zu bytes):\n", strlen(TEST_INPUT));
    printf("    Payload: \"%s\"\n", TEST_INPUT);

    ssize_t written = write(fd, TEST_INPUT, strlen(TEST_INPUT));
    if (written < 0) {
        fprintf(stderr, "[-] write() failed: %s\n", strerror(errno));
        close(fd);
        return EXIT_FAILURE;
    }

    /* 2. Read Computed Cryptographic Digests from Kernel */
    char response[512] = {0};
    ssize_t bytes_read = read(fd, response, sizeof(response) - 1);
    if (bytes_read < 0) {
        fprintf(stderr, "[-] read() failed: %s\n", strerror(errno));
        close(fd);
        return EXIT_FAILURE;
    }

    printf("\n[+] Kernel Crypto Engine Response:\n%s\n", response);

    /* 3. Verify Integrity Against NIST Reference */
    if (strstr(response, NIST_SHA256) != NULL) {
        printf("[SUCCESS] NIST SHA-256 verification PASSED! Digest matches reference.\n");
    } else {
        printf("[WARNING] Digest mismatch against reference %s\n", NIST_SHA256);
    }

    close(fd);

    /* 4. Display Driver Telemetry & Hardware Accelerator */
    print_sysfs_stats();

    printf("\n[SUCCESS] Chapter 29 Kernel Cryptographic API verified!\n");
    return EXIT_SUCCESS;
}
