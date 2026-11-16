# Chapter 29: Linux Kernel Cryptographic API

## 1. Architectural Overview of the Linux Crypto Framework

The **Linux Cryptographic API** (`crypto/`) provides a unified, extensible architecture supporting hardware and software implementations of ciphers, message digests, AEAD (Authenticated Encryption with Associated Data), random number generators, and compression algorithms.

Subsystems throughout the kernel rely directly on the Crypto API:
- **IPsec & WireGuard**: Packet encryption, hashing, and HMAC verification.
- **dm-crypt / LUKS**: Full disk encryption (XTS-AES).
- **Kernel Module Verification**: Verifying RSA / ECDSA signatures on loaded modules.
- **FS-Crypt**: Ext4/F2FS transparent per-file encryption.

```
+--------------------------------------------------------------------+
|                    Kernel Consumers / Subsystems                   |
|       (dm-crypt, IPsec, Module Loader, /dev/kernel_crypto)         |
+--------------------------------------------------------------------+
                                  |
                                  v
+--------------------------------------------------------------------+
|                    Linux Crypto API Core Engine                    |
|      - Transform Allocation (crypto_alloc_shash, crypto_alloc_aead)|
|      - Priority-Based Driver Matching (crypto_alg registry)        |
+--------------------------------------------------------------------+
                                  |
         +------------------------+------------------------+
         |                                                 |
         v                                                 v
+-----------------------------+               +-----------------------------+
|   Software Implementation   |               |   Hardware Acceleration     |
|   (sha256-generic, C code)  |               |  (sha256-avx2, Intel QAT,   |
|                             |               |   ARMv8 Crypto Extensions)  |
+-----------------------------+               +-----------------------------+
```

---

## 2. Synchronous vs Asynchronous Hash APIs

The kernel provides two primary interfaces for hashing:

| Feature | Synchronous Hash (`crypto_shash`) | Asynchronous Hash (`crypto_ahash`) |
|---|---|---|
| **Memory Buffer** | Direct linear contiguous byte pointers (`const u8 *data`) | Scatter-Gather lists (`struct scatterlist *sg`) |
| **Execution** | Synchronous, in-place on current CPU | Can execute asynchronously on external DMA engines |
| **Allocation** | `crypto_alloc_shash("sha256", 0, 0)` | `crypto_alloc_ahash("sha256", 0, 0)` |
| **Descriptor** | `struct shash_desc` with `crypto_shash_descsize` | `struct ahash_request` with completion callback |
| **Use Case** | Small buffers, in-memory checks, low overhead | High-throughput streaming, bulk storage encryption |

---

## 3. The `shash_desc` Lifecycle

Unlike user-space OpenSSL where contexts are opaque handles, kernel `shash_desc` structures require inline memory allocation:

```c
/* 1. Calculate required context size */
unsigned int size = sizeof(struct shash_desc) + crypto_shash_descsize(tfm);

/* 2. Allocate context buffer */
struct shash_desc *desc = kzalloc(size, GFP_KERNEL);
desc->tfm = tfm;

/* 3. Compute digest in a single atomic pass */
crypto_shash_digest(desc, input_data, input_len, out_digest);

/* 4. Release context */
kfree(desc);
```

### For Multi-Part Incremental Hashing:
```c
crypto_shash_init(desc);
crypto_shash_update(desc, chunk1, len1);
crypto_shash_update(desc, chunk2, len2);
crypto_shash_final(desc, out_digest);
```

---

## 4. Hardware Priority and Optimization

Each algorithm registered in the kernel declares a priority (`cra_priority`):
- `sha256-generic` (C code): priority = 100
- `sha256-x86_64` (Assembly): priority = 150
- `sha256-avx2` (SIMD Vectorized): priority = 300
- `sha256-ni` (Hardware Intel SHA Extensions): priority = 400

When your driver calls `crypto_alloc_shash("sha256", 0, 0)`, the kernel automatically binds to the highest-priority implementation available on the running processor without any code changes!

---

## 5. Summary & Best Practices

- Always allocate descriptors with `sizeof(struct shash_desc) + crypto_shash_descsize(tfm)`.
- Free all allocated transforms during module unload with `crypto_free_shash(tfm)`.
- Use keyed hashes (`hmac(sha256)`) with `crypto_shash_setkey(tfm, key, keylen)` for cryptographically secure message authentication.
