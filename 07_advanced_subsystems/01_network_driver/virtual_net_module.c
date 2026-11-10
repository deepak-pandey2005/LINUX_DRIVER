// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file virtual_net_module.c
 * @brief Chapter 21: Network Device Driver Subsystem (net_device, sk_buff, RX/TX)
 * @author Deepak Pandey
 *
 * This module demonstrates:
 *  - Architecture of the Linux Network Subsystem (struct net_device).
 *  - Allocating Ethernet devices with alloc_etherdev() and assigning random MAC addresses (eth_hw_addr_random).
 *  - Implementing struct net_device_ops (ndo_open, ndo_stop, ndo_start_xmit, ndo_get_stats64).
 *  - Socket buffer manipulation (struct sk_buff, dev_kfree_skb, skb_clone, eth_type_trans).
 *  - In-kernel packet transmission (TX) and reception loopback injection (netif_rx).
 *  - Queue and link carrier management (netif_start_queue, netif_carrier_on).
 *  - Inspecting interface state via standard Linux network tools (ip link, ifconfig).
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/spinlock.h>
#include <linux/ip.h>

#define DRIVER_NAME "vnet_driver"
#define IFACE_NAME  "vnet0"

/**
 * @brief Driver private state structure
 */
struct vnet_priv {
    struct net_device *netdev;
    spinlock_t lock;

    /* Interface 64-bit telemetry statistics */
    u64 tx_packets;
    u64 tx_bytes;
    u64 rx_packets;
    u64 rx_bytes;
    u64 rx_dropped;
};

static int vnet_open(struct net_device *dev)
{
    pr_info("Interface '%s' brought UP (ndo_open)\n", dev->name);
    netif_carrier_on(dev);
    netif_start_queue(dev);
    return 0;
}

static int vnet_stop(struct net_device *dev)
{
    pr_info("Interface '%s' brought DOWN (ndo_stop)\n", dev->name);
    netif_stop_queue(dev);
    netif_carrier_off(dev);
    return 0;
}

/**
 * @brief Transmit packet handler (ndo_start_xmit)
 * Called by network stack when a user application sends a packet.
 */
static netdev_tx_t vnet_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
    struct vnet_priv *priv = netdev_priv(dev);
    struct sk_buff *rx_skb;
    unsigned int len = skb->len;
    unsigned long flags;

    /* Update TX statistics */
    spin_lock_irqsave(&priv->lock, flags);
    priv->tx_packets++;
    priv->tx_bytes += len;
    spin_unlock_irqrestore(&priv->lock, flags);

    /*
     * Simulate virtual loopback reception:
     * Clone the outgoing packet and inject it back into the kernel's network stack!
     */
    rx_skb = skb_clone(skb, GFP_ATOMIC);
    if (rx_skb) {
        /* Set receiving device */
        rx_skb->dev = dev;
        rx_skb->protocol = eth_type_trans(rx_skb, dev);
        rx_skb->ip_summed = CHECKSUM_UNNECESSARY;

        /* Inject into Linux RX softirq layer */
        netif_rx(rx_skb);

        spin_lock_irqsave(&priv->lock, flags);
        priv->rx_packets++;
        priv->rx_bytes += len;
        spin_unlock_irqrestore(&priv->lock, flags);
    } else {
        spin_lock_irqsave(&priv->lock, flags);
        priv->rx_dropped++;
        spin_unlock_irqrestore(&priv->lock, flags);
    }

    /* Free original transmitted socket buffer */
    dev_kfree_skb(skb);

    return NETDEV_TX_OK;
}

static void vnet_get_stats64(struct net_device *dev, struct rtnl_link_stats64 *storage)
{
    struct vnet_priv *priv = netdev_priv(dev);
    unsigned long flags;

    spin_lock_irqsave(&priv->lock, flags);
    storage->tx_packets = priv->tx_packets;
    storage->tx_bytes   = priv->tx_bytes;
    storage->rx_packets = priv->rx_packets;
    storage->rx_bytes   = priv->rx_bytes;
    storage->rx_dropped = priv->rx_dropped;
    spin_unlock_irqrestore(&priv->lock, flags);
}

static const struct net_device_ops vnet_netdev_ops = {
    .ndo_open        = vnet_open,
    .ndo_stop        = vnet_stop,
    .ndo_start_xmit  = vnet_start_xmit,
    .ndo_get_stats64 = vnet_get_stats64,
    .ndo_set_mac_address = eth_mac_addr,
    .ndo_validate_addr   = eth_validate_addr,
};

static struct net_device *g_vnet_dev;

static int __init vnet_init(void)
{
    int ret;
    struct vnet_priv *priv;

    pr_info("Initializing Chapter 21 Virtual Network Device Driver\n");

    /* 1. Allocate standard Ethernet net_device with private storage */
    g_vnet_dev = alloc_etherdev(sizeof(struct vnet_priv));
    if (!g_vnet_dev) {
        pr_err("alloc_etherdev failed\n");
        return -ENOMEM;
    }

    strscpy(g_vnet_dev->name, IFACE_NAME, IFNAMSIZ);
    g_vnet_dev->netdev_ops = &vnet_netdev_ops;

    /* Assign random locally-administered MAC address */
    eth_hw_addr_random(g_vnet_dev);

    /* Initialize private data */
    priv = netdev_priv(g_vnet_dev);
    priv->netdev = g_vnet_dev;
    spin_lock_init(&priv->lock);
    priv->tx_packets = 0;
    priv->tx_bytes   = 0;
    priv->rx_packets = 0;
    priv->rx_bytes   = 0;
    priv->rx_dropped = 0;

    /* 2. Register net_device with Linux Network Core */
    ret = register_netdev(g_vnet_dev);
    if (ret) {
        pr_err("register_netdev failed (%d)\n", ret);
        free_netdev(g_vnet_dev);
        return ret;
    }

    pr_info("Virtual network interface '%s' registered with MAC %pM\n",
            g_vnet_dev->name, g_vnet_dev->dev_addr);
    return 0;
}

static void __exit vnet_exit(void)
{
    pr_info("Unloading Virtual Network Device Driver\n");

    if (g_vnet_dev) {
        unregister_netdev(g_vnet_dev);
        free_netdev(g_vnet_dev);
        g_vnet_dev = NULL;
    }

    pr_info("Interface destroyed and resources freed\n");
}

module_init(vnet_init);
module_exit(vnet_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Deepak Pandey");
MODULE_DESCRIPTION("Chapter 21: Network Device Driver Subsystem (net_device, sk_buff, loopback)");
MODULE_VERSION("1.0");
