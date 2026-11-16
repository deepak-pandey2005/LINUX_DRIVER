# Chapter 27: Watchdog Timer Subsystem

## 1. Architectural Overview of the Watchdog Framework

A **Watchdog Timer (WDT)** is a countdown timer that hardware resets the processor if software fails to service ("pet" or "kick") the watchdog within a predetermined deadline.

In mission-critical embedded systems and enterprise servers, the watchdog is the last line of defense against:
- Deadlocks or infinite loops in user space or kernel drivers.
- Memory exhaustion causing critical service starvation.
- Unhandled kernel freezes and scheduler stalls.

```
+--------------------------------------------------------------------+
|                    User Space Applications                         |
|             (systemd watchdog, watchdogd, custom daemon)            |
+--------------------------------------------------------------------+
                                  |
                                  v
+--------------------------------------------------------------------+
|                   Linux Watchdog Core Subsystem                    |
|      - Character Device Creation (/dev/watchdog, /dev/watchdog0)   |
|      - ioctl Command Dispatch (WDIOC_KEEPALIVE, WDIOC_SETTIMEOUT)  |
|      - Magic Close Enforcement ('V' protocol)                      |
+--------------------------------------------------------------------+
                                  |
                                  v
+--------------------------------------------------------------------+
|               Watchdog Driver (mock_watchdog)                      |
|      - struct watchdog_device, struct watchdog_ops                 |
|      - Callbacks: .start, .stop, .ping, .set_timeout, .get_timeleft|
+--------------------------------------------------------------------+
                                  |
                                  v
+--------------------------------------------------------------------+
|              Physical Hardware WDT / PMIC Countdown                |
+--------------------------------------------------------------------+
```

---

## 2. The Core Watchdog Interfaces

### 2.1 `struct watchdog_device`
The central kernel representation registered with `devm_watchdog_register_device(parent, &wdd)`:
- `wdd->info`: Hardware identity string and capability flags (`struct watchdog_info`).
- `wdd->ops`: Driver dispatch operations table (`struct watchdog_ops`).
- `wdd->timeout`: Active timeout in seconds.
- `wdd->min_timeout` / `wdd->max_timeout`: Allowed timeout boundary limits.

### 2.2 `struct watchdog_ops`
```c
static const struct watchdog_ops wdt_ops = {
    .owner        = THIS_MODULE,
    .start        = wdt_start,
    .stop         = wdt_stop,
    .ping         = wdt_ping,
    .set_timeout  = wdt_set_timeout,
    .get_timeleft = wdt_get_timeleft,
};
```

---

## 3. The Magic Close Safety Protocol

A naive watchdog driver that stops whenever `/dev/watchdog` is closed presents a critical safety hazard: if the monitoring daemon crashes or is terminated by `SIGKILL` (e.g. out-of-memory killer), the device node would close normally and disarm the watchdog—leaving the system hung indefinitely without resetting!

To prevent this, the Linux watchdog subsystem implements **Magic Close (`WDIOF_MAGICCLOSE`)**:
1. When `/dev/watchdog` is opened, the watchdog starts and countdown begins.
2. If the file is closed **without** writing the magic character `'V'`, the kernel leaves the watchdog **running**. If no subsequent process opens the node and pets it, the system resets!
3. To disarm the watchdog intentionally, the closing process must write `'V'` into the file descriptor immediately prior to `close()`.

```c
/* Graceful intentional shutdown */
write(fd, "V", 1);
close(fd);
```

---

## 4. Standard Watchdog IOCTLs

| IOCTL Macro | Description |
|---|---|
| `WDIOC_GETSUPPORT` | Returns `struct watchdog_info` capabilities and identity |
| `WDIOC_KEEPALIVE` | Pets the watchdog, restarting countdown from `timeout` |
| `WDIOC_SETTIMEOUT` | Sets a new countdown duration in seconds |
| `WDIOC_GETTIMEOUT` | Reads current configured timeout |
| `WDIOC_GETTIMELEFT` | Reads remaining seconds before system reset |

---

## 5. Summary & Best Practices

- Always validate requested timeouts against `min_timeout` and `max_timeout`.
- In production drivers, use the kernel core watchdog framework (`CONFIG_WATCHDOG_CORE=y`) rather than reinventing custom character devices.
- For high-reliability applications, integrate with `systemd` watchdog support via `WatchdogSec=` in unit files.
