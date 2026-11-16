# Chapter 29: Linux Kernel Cryptographic API

## Overview
This lab demonstrates the architecture and programming model of the **Linux Kernel Cryptographic API**. You will learn how the kernel abstracts cryptographic primitives, allocates synchronous hash transforms (`struct crypto_shash`), computes digests (SHA-256) and keyed authenticators (HMAC-SHA256), utilizes hardware SIMD/accelerated drivers (such as `sha256-avx2`), and verifies outputs against standard NIST CAVP test vectors.

## Key Concepts
- **Linux Crypto Architecture**:
  - **Cipher Transforms (`crypto_tfm`)**: Instantiations of algorithms with specific parameters.
  - **Synchronous Hashes (`crypto_shash`)**: Block-by-block synchronous digests (`crypto_alloc_shash`, `crypto_shash_digest`).
  - **Asynchronous Hashes (`crypto_ahash`)**: Scatterlist-based asynchronous hashing for dedicated cryptographic co-processors.
  - **Symmetric Ciphers (`crypto_skcipher`)**: Block and stream ciphers (AES-CBC, AES-XTS).
- **Execution Workflow**:
  1. `tfm = crypto_alloc_shash("sha256", 0, 0);`
  2. `desc = kzalloc(sizeof(*desc) + crypto_shash_descsize(tfm), GFP_KERNEL);`
  3. `desc->tfm = tfm;`
  4. `crypto_shash_digest(desc, data, len, out);`
  5. `crypto_free_shash(tfm);`
- **Hardware Acceleration Discovery**:
  - The kernel automatically selects the highest-priority driver matching the algorithm name (e.g., AVX2/AVX-512 vector implementations or hardware engines like Intel QAT/ARMv8 CE).

## Building and Running

### 1. Build Module and Client
```bash
make
```

### 2. Load the Module
```bash
sudo insmod crypto_api_module.ko
dmesg | tail -n 10
```

### 3. Grant Permissions to Device Node
```bash
sudo chmod 666 /dev/kernel_crypto
```

### 4. Run the Verification Client
```bash
./test_client
```

### 5. Inspect Driver Telemetry Directly
```bash
cat /sys/class/kernel_crypto_class/kernel_crypto/crypto_stats
```

### 6. Clean Up
```bash
sudo rmmod crypto_api_module
make clean
```
