# Chapter 21: Network Device Driver Subsystem

## Overview
This lab covers the architecture and implementation of Linux Network Device Drivers. You will learn how the kernel manages network interfaces, transmits and receives packets using Socket Buffers (`sk_buff`), registers device operations (`net_device_ops`), and exposes statistics via `rtnl_link_stats64`.

## Key Concepts
- **`struct net_device`**: Core abstraction representing a network interface in the Linux kernel.
- **`alloc_etherdev()`**: Allocates an Ethernet interface device initialized with standard defaults (`ether_setup`).
- **`net_device_ops`**: Operation callbacks including `.ndo_open`, `.ndo_stop`, `.ndo_start_xmit`, and `.ndo_get_stats64`.
- **`sk_buff` (Socket Buffer)**: Kernel packet container navigating the entire TCP/IP stack from socket to hardware.
- **`netif_rx()` & Softirq Injection**: Feeding received packets back into the kernel network queue asynchronously.
- **Sysfs & Netlink Inspection**: Tracking interface telemetry through `/sys/class/net/vnet0/` and `iproute2`.

## Building and Testing

### 1. Build the Module and User Client
```bash
make
```

### 2. Load the Kernel Module
```bash
sudo insmod virtual_net_module.ko
dmesg | tail -n 10
```

### 3. Verify Interface Registration
```bash
ip link show vnet0
./test_client
```

### 4. Configure Interface and Send Traffic
```bash
# Bring interface UP
sudo ip link set vnet0 up

# Assign an IP address
sudo ip addr add 192.168.42.10/24 dev vnet0

# Ping the virtual interface (packets loop back through netif_rx)
ping -c 4 192.168.42.10

# Check updated telemetry statistics
./test_client
```

### 5. Cleanup
```bash
sudo rmmod virtual_net_module
make clean
```

