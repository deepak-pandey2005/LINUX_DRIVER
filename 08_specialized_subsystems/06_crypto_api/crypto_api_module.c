// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file crypto_api_module.c
 * @brief Chapter 29: Linux Kernel Cryptographic API (crypto_shash, SHA-256, HMAC)
 * @author Deepak Pandey
 *
 * This module demonstrates:
 *  - Architecture of the Linux Kernel Crypto Framework.
 *  - Allocating synchronous hash transforms (struct crypto_shash, crypto_alloc_shash).
 *  - Managing shash descriptor contexts (struct shash_desc, crypto_shash_descsize).
 *  - Calculating standard cryptographic digests (SHA-256) via crypto_shash_digest().
 *  - Keyed-hash message authentication codes (HMAC-SHA256) via crypto_shash_setkey().
 *  - Benchmarking hardware/software crypto execution latency using ktime_get_ns().
 *  - Exposing character device (/dev/kernel_crypto) and sysfs telemetry attributes.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/crypto.h>
#include <crypto/hash.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/ktime.h>
#include <linux/mutex.h>
#include <linux/device.h>

#define DRIVER_NAME      "kernel_crypto_module"
#define DEV_NAME         "kernel_crypto"
#define SHA256_DIGEST_SZ 32
#define BUFFER_SIZE      4096

struct crypto_demo_priv {
    struct crypto_shash *sha256_tfm;
    struct crypto_shash *hmac_tfm;

    dev_t dev_num;
    struct cdev cdev;
    struct class *class;
    struct device *device;
    struct mutex lock;

    /* Cached input and digest state */
    u8 in_buffer[BUFFER_SIZE];
    size_t in_len;
    u8 sha256_digest[SHA256_DIGEST_SZ];
    u8 hmac_digest[SHA256_DIGEST_SZ];
    u64 last_hash_ns;

    /* Telemetry statistics */
    u64 total_hashes;
    u64 total_bytes_hashed;
};

static struct crypto_demo_priv *g_crypto_priv;

/**
 * @brief Computes SHA-256 digest for input buffer
 */
static int calc_sha256(struct crypto_shash *tfm, const u8 *data, size_t len, u8 *out_digest)
{
    unsigned int desc_size = sizeof(struct shash_desc) + crypto_shash_descsize(tfm);
    struct shash_desc *desc;
    int ret;

    desc = kzalloc(desc_size, GFP_KERNEL);
    if (!desc)
        return -ENOMEM;

    desc->tfm = tfm;
    ret = crypto_shash_digest(desc, data, len, out_digest);
    kfree(desc);

    return ret;
}

/**
 * @brief Computes HMAC-SHA256 digest with a secret key
 */
static int calc_hmac_sha256(struct crypto_shash *tfm, const u8 *key, size_t keylen,
                            const u8 *data, size_t len, u8 *out_digest)
{
    unsigned int desc_size = sizeof(struct shash_desc) + crypto_shash_descsize(tfm);
    struct shash_desc *desc;
    int ret;

    ret = crypto_shash_setkey(tfm, key, keylen);
    if (ret)
        return ret;

    desc = kzalloc(desc_size, GFP_KERNEL);
    if (!desc)
        return -ENOMEM;

    desc->tfm = tfm;
    ret = crypto_shash_digest(desc, data, len, out_digest);
    kfree(desc);

    return ret;
}

static int crypto_demo_open(struct inode *inode, struct file *file)
{
    file->private_data = g_crypto_priv;
    return 0;
}

static int crypto_demo_release(struct inode *inode, struct file *file)
{
    return 0;
}

static ssize_t crypto_demo_write(struct file *file, const char __user *user_buf,
                                 size_t count, loff_t *ppos)
{
    struct crypto_demo_priv *priv = file->private_data;
    size_t to_copy;
    ktime_t start_time, end_time;
    static const u8 default_key[] = "AntigravityKernelKeySecret2026!";
    int ret;

    if (!priv || !priv->sha256_tfm)
        return -ENODEV;

    to_copy = min(count, (size_t)BUFFER_SIZE);

    if (mutex_lock_interruptible(&priv->lock))
        return -ERESTARTSYS;

    if (copy_from_user(priv->in_buffer, user_buf, to_copy)) {
        mutex_unlock(&priv->lock);
        return -EFAULT;
    }

    priv->in_len = to_copy;

    /* Measure hashing execution time */
    start_time = ktime_get();
    ret = calc_sha256(priv->sha256_tfm, priv->in_buffer, priv->in_len, priv->sha256_digest);
    if (ret) {
        mutex_unlock(&priv->lock);
        return ret;
    }

    /* Compute HMAC */
    ret = calc_hmac_sha256(priv->hmac_tfm, default_key, sizeof(default_key) - 1,
                           priv->in_buffer, priv->in_len, priv->hmac_digest);
    end_time = ktime_get();

    priv->last_hash_ns = ktime_to_ns(ktime_sub(end_time, start_time));
    priv->total_hashes += 2; /* SHA256 + HMAC */
    priv->total_bytes_hashed += to_copy;

    pr_info("Hashed %zu bytes in %llu ns. SHA-256: %02x%02x%02x%02x...\n",
            to_copy, priv->last_hash_ns,
            priv->sha256_digest[0], priv->sha256_digest[1],
            priv->sha256_digest[2], priv->sha256_digest[3]);

    mutex_unlock(&priv->lock);
    return to_copy;
}

static ssize_t crypto_demo_read(struct file *file, char __user *user_buf,
                                size_t count, loff_t *ppos)
{
    struct crypto_demo_priv *priv = file->private_data;
    char out[512];
    int pos = 0;
    int i;
    size_t out_len;

    if (!priv)
        return -ENODEV;

    if (mutex_lock_interruptible(&priv->lock))
        return -ERESTARTSYS;

    pos += scnprintf(out + pos, sizeof(out) - pos, "SHA-256: ");
    for (i = 0; i < SHA256_DIGEST_SZ; ++i)
        pos += scnprintf(out + pos, sizeof(out) - pos, "%02x", priv->sha256_digest[i]);
    pos += scnprintf(out + pos, sizeof(out) - pos, "\nHMAC-256: ");
    for (i = 0; i < SHA256_DIGEST_SZ; ++i)
        pos += scnprintf(out + pos, sizeof(out) - pos, "%02x", priv->hmac_digest[i]);
    pos += scnprintf(out + pos, sizeof(out) - pos, "\nLatency: %llu ns\n", priv->last_hash_ns);

    out_len = pos;

    if (*ppos >= out_len) {
        mutex_unlock(&priv->lock);
        return 0;
    }

    if (count < out_len - *ppos)
        out_len = count + *ppos;

    if (copy_to_user(user_buf, out + *ppos, out_len - *ppos)) {
        mutex_unlock(&priv->lock);
        return -EFAULT;
    }

    out_len -= *ppos;
    *ppos += out_len;
    mutex_unlock(&priv->lock);

    return out_len;
}

static const struct file_operations crypto_fops = {
    .owner   = THIS_MODULE,
    .open    = crypto_demo_open,
    .release = crypto_demo_release,
    .read    = crypto_demo_read,
    .write   = crypto_demo_write,
};

/* Sysfs Statistics Attribute */
static ssize_t crypto_stats_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct crypto_demo_priv *priv = g_crypto_priv;

    if (!priv)
        return -ENODEV;

    return sysfs_emit(buf,
                      "Crypto Algorithm:   sha256, hmac(sha256)\n"
                      "Driver Name:        %s\n"
                      "Digest Size:        %u bytes\n"
                      "Total Hashes:       %llu\n"
                      "Total Bytes:        %llu bytes\n"
                      "Last Execution:     %llu ns\n",
                      crypto_tfm_alg_driver_name(crypto_shash_tfm(priv->sha256_tfm)),
                      crypto_shash_digestsize(priv->sha256_tfm),
                      priv->total_hashes,
                      priv->total_bytes_hashed,
                      priv->last_hash_ns);
}
static DEVICE_ATTR_RO(crypto_stats);

static int __init crypto_demo_init(void)
{
    int ret;
    struct crypto_demo_priv *priv;

    pr_info("Initializing Chapter 29 Kernel Cryptographic API Module\n");

    priv = kzalloc(sizeof(*priv), GFP_KERNEL);
    if (!priv)
        return -ENOMEM;

    mutex_init(&priv->lock);

    /* 1. Allocate SHA-256 and HMAC-SHA256 transforms */
    priv->sha256_tfm = crypto_alloc_shash("sha256", 0, 0);
    if (IS_ERR(priv->sha256_tfm)) {
        ret = PTR_ERR(priv->sha256_tfm);
        pr_err("crypto_alloc_shash(sha256) failed: %d\n", ret);
        goto err_kfree;
    }

    priv->hmac_tfm = crypto_alloc_shash("hmac(sha256)", 0, 0);
    if (IS_ERR(priv->hmac_tfm)) {
        ret = PTR_ERR(priv->hmac_tfm);
        pr_err("crypto_alloc_shash(hmac(sha256)) failed: %d\n", ret);
        goto err_free_sha256;
    }

    /* 2. Register character device */
    ret = alloc_chrdev_region(&priv->dev_num, 0, 1, DEV_NAME);
    if (ret < 0) {
        pr_err("alloc_chrdev_region failed (%d)\n", ret);
        goto err_free_hmac;
    }

    cdev_init(&priv->cdev, &crypto_fops);
    priv->cdev.owner = THIS_MODULE;

    ret = cdev_add(&priv->cdev, priv->dev_num, 1);
    if (ret < 0) {
        pr_err("cdev_add failed (%d)\n", ret);
        goto err_chrdev;
    }

    priv->class = class_create("kernel_crypto_class");
    if (IS_ERR(priv->class)) {
        ret = PTR_ERR(priv->class);
        goto err_cdev;
    }

    priv->device = device_create(priv->class, NULL, priv->dev_num, NULL, DEV_NAME);
    if (IS_ERR(priv->device)) {
        ret = PTR_ERR(priv->device);
        goto err_class;
    }

    ret = device_create_file(priv->device, &dev_attr_crypto_stats);
    if (ret)
        pr_warn("device_create_file failed (%d)\n", ret);

    g_crypto_priv = priv;
    pr_info("Crypto driver ready! Underlying driver: %s. Node: /dev/%s\n",
            crypto_tfm_alg_driver_name(crypto_shash_tfm(priv->sha256_tfm)), DEV_NAME);

    return 0;

err_class:
    class_destroy(priv->class);
err_cdev:
    cdev_del(&priv->cdev);
err_chrdev:
    unregister_chrdev_region(priv->dev_num, 1);
err_free_hmac:
    crypto_free_shash(priv->hmac_tfm);
err_free_sha256:
    crypto_free_shash(priv->sha256_tfm);
err_kfree:
    kfree(priv);
    return ret;
}

static void __exit crypto_demo_exit(void)
{
    pr_info("Unloading Chapter 29 Kernel Cryptographic API Module\n");

    if (g_crypto_priv) {
        device_remove_file(g_crypto_priv->device, &dev_attr_crypto_stats);
        device_destroy(g_crypto_priv->class, g_crypto_priv->dev_num);
        class_destroy(g_crypto_priv->class);
        cdev_del(&g_crypto_priv->cdev);
        unregister_chrdev_region(g_crypto_priv->dev_num, 1);

        crypto_free_shash(g_crypto_priv->hmac_tfm);
        crypto_free_shash(g_crypto_priv->sha256_tfm);
        kfree(g_crypto_priv);
        g_crypto_priv = NULL;
    }

    pr_info("Crypto driver unloaded cleanly\n");
}

module_init(crypto_demo_init);
module_exit(crypto_demo_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Deepak Pandey");
MODULE_DESCRIPTION("Chapter 29: Linux Kernel Cryptographic API (crypto_shash, SHA-256, HMAC)");
MODULE_VERSION("1.0");
