# Chapter 21: Network Device Driver Subsystem

## 1. Architectural Overview

The Linux Network Subsystem decouples hardware device drivers from transport protocols (TCP, UDP, ICMP) and network-layer routing (IPv4, IPv6) through a unified device abstraction: **`struct net_device`**.

Unlike character or block drivers, network drivers do not create device nodes in `/dev`. Instead, they register network interfaces with the kernel core (`register_netdev`), which exposes them through the Netlink API, `sysfs` (`/sys/class/net/`), and standard user-space tooling like `ip`, `ifconfig`, and `ethtool`.

```
+--------------------------------------------------------------------+
|                        User Space Applications                     |
|            (Sockets: AF_INET, AF_INET6, AF_PACKET, Raw)            |
+--------------------------------------------------------------------+
                                  |
                                  v
+--------------------------------------------------------------------+
|                       Linux Kernel Network Core                    |
|         Routing Engine / Netfilter / Transport (TCP/UDP)           |
+--------------------------------------------------------------------+
                                  |
               Transmit Path      |      Receive Path
                     v            |            ^
           +--------------------+ | +--------------------+
           |   ndo_start_xmit   | | |     netif_rx()     |
           +--------------------+ | +--------------------+
                     |            |            ^
                     +----------->+------------+ (Simulated Loopback)
                                  |
+--------------------------------------------------------------------+
|                   vnet_driver (struct net_device)                  |
+--------------------------------------------------------------------+
```

---

## 2. The Core Data Structures

### 2.1 `struct net_device`
Allocated via helper functions like `alloc_etherdev(sizeof(struct vnet_priv))` (or generic `alloc_netdev_mqs()`), this structure encapsulates:
- Hardware interface properties: MAC address (`dev_addr`), MTU, broadcast address, flags (`IFF_UP`, `IFF_BROADCAST`, `IFF_MULTICAST`).
- Private driver context: accessed via `netdev_priv(dev)`.
- Operations dispatch table: `struct net_device_ops`.

### 2.2 `struct sk_buff` (Socket Buffer)
The socket buffer is the fundamental data structure used throughout the Linux networking stack:
- `skb->head`, `skb->data`, `skb->tail`, `skb->end`: Pointer boundaries for packet payload manipulation without memory copying.
- `skb->len`: Total packet byte length.
- `skb->protocol`: Network layer protocol ID (e.g., `ETH_P_IP`).
- `dev_kfree_skb()`: Releases the socket buffer once transmission or processing is complete.

---

## 3. Network Device Operations (`net_device_ops`)

| Callback | Purpose | Typical Actions |
|---|---|---|
| `ndo_open` | Interface activation (`ip link set dev up`) | Enable hardware IRQs, allocate RX buffers, `netif_carrier_on()`, `netif_start_queue()` |
| `ndo_stop` | Interface deactivation (`ip link set dev down`) | `netif_stop_queue()`, `netif_carrier_off()`, free hardware ring buffers |
| `ndo_start_xmit` | Packet transmission | Enqueue skb into TX ring buffer, notify hardware DMA, or clone/loopback |
| `ndo_get_stats64` | Statistics querying | Fill `struct rtnl_link_stats64` with packet, byte, and error counters |

---

## 4. Transmission & Reception Loopback Flow

In this chapter's driver (`virtual_net_module.c`):
1. **TX Initiation**: The network stack invokes `vnet_start_xmit(skb, dev)`.
2. **Telemetry Update**: TX packets and bytes are incremented under `spin_lock_irqsave`.
3. **Loopback Duplication**: `skb_clone(skb, GFP_ATOMIC)` creates a zero-copy reference clone of the payload.
4. **Header Translation**: `eth_type_trans(rx_skb, dev)` identifies Ethernet protocol types and strips the L2 header.
5. **RX Softirq Injection**: `netif_rx(rx_skb)` posts the received packet to the CPU's backlog queue, scheduling `NET_RX_SOFTIRQ` for processing.
6. **TX Buffer Disposal**: `dev_kfree_skb(skb)` safely frees the original transmission buffer.

---

## 5. Summary & Best Practices

- Always protect statistics updates with spinlocks or use per-CPU 64-bit sequence counters (`u64_stats_t`) for lockless high-throughput telemetry.
- Use `GFP_ATOMIC` inside transmission contexts (`ndo_start_xmit`), as packet sending may occur in softirq or atomic contexts.
- Handle queue throttling with `netif_stop_queue(dev)` when hardware descriptor rings fill up, and `netif_wake_queue(dev)` when ring slots become available again.
