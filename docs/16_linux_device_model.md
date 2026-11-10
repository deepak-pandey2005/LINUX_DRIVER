# Chapter 16: Linux Device Model & Platform Drivers

The **Linux Device Model (LDM)** was introduced to provide an object-oriented, hierarchical representation of all hardware and software abstractions in the operating system.

---

## 1. The Core Triad: Bus, Device, Driver

Every entity in the Linux Device Model belongs to one of three foundational classes:

```mermaid
graph TD
    Bus["struct bus_type (e.g. platform_bus_type, pci_bus_type)"]
    Device["struct device (Hardware Instance)"]
    Driver["struct device_driver (Software Controller)"]

    Bus -->|Matches by name, OF, ACPI, or ID| Device
    Bus -->|Registers & Discovers| Driver
    Device <-->|probe() binds them / remove() unbinds| Driver
```

1. **Bus (`struct bus_type`)**: Defines the channel of communication (PCI, USB, I2C, SPI, Platform). The bus provides a `match()` callback comparing device attributes with driver ID tables.
2. **Device (`struct device`)**: Represents an actual hardware peripheral (or simulated device) attached to the bus.
3. **Driver (`struct device_driver`)**: Represents the software module capable of driving a device.

When a device is added, the bus scans registered drivers. If `bus->match(dev, drv)` returns non-zero, the bus calls `drv->probe(dev)`.

---

## 2. Platform Bus (`platform_bus_type`)

Unlike PCI or USB devices that can be enumerated through vendor/device IDs over hardware protocol handshakes, system-on-chip (SoC) peripherals are hard-wired to memory addresses.

The **Platform Bus** is a pseudo-bus that models these non-enumerable devices:
- Memory resources (`IORESOURCE_MEM`).
- Interrupt resources (`IORESOURCE_IRQ`).
- DMA channels (`IORESOURCE_DMA`).

---

## 3. Platform Driver API & Modern Conventions

```c
struct platform_driver {
    int (*probe)(struct platform_device *pdev);
    void (*remove)(struct platform_device *pdev);
    struct device_driver driver;
    const struct platform_device_id *id_table;
};
```

### 3.1 Resource Extraction
Drivers retrieve memory windows and IRQs using helper functions:
```c
struct resource *res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
int irq = platform_get_irq(pdev, 0);
```

### 3.2 Managed Resource Allocation (`devres`)
Modern drivers avoid manual memory freeing by using device-managed allocators:
```c
struct my_priv *priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
```
Memory allocated with `devm_*` is automatically reclaimed when the device is unbound or if `probe()` fails!

### 3.3 Driver Private Data
```c
platform_set_drvdata(pdev, priv);
struct my_priv *priv = platform_get_drvdata(pdev);
```
