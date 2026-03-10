# Log -001: Hardware Diagnostics & NIC Deployment

**Date:** 2026-02-16 to 02-20  
**Hardware:** Solarflare SN7122F (Dual-Port 10GbE)  
**Focus:** Hardware Provisioning, Kernel Bypass, Firmware Architecture, PCIe Topology

This log documents the provisioning and diagnostic process for the primary Network Interface Card (NIC). The original architectural goal was to utilize the proprietary OpenOnload enterprise drivers to expose the `ef_vi` API for true zero-copy kernel bypass.

## Table of Contents
1. [Firmware & Memory Layout Architecture](#firmware--memory-layout-architecture)
2. [Diagnostic Path & MC Corruption](#diagnostic-path--mc-corruption)
3. [PCIe Topology & Bifurcation Optimization](#pcie-topology--bifurcation-optimization)
4. [Current Hardware Status & Software Fallback](#current-hardware-status--software-fallback)

## Firmware & Memory Layout Architecture

* **The `ef_vi` Constraint:** While the upstream `sfc` driver in the standard Linux kernel provides baseline connectivity, achieving sub-microsecond latency requires bypassing the kernel's network stack entirely using OpenOnload/`ef_vi`.
* **The Partition Shift:** Investigation into the card's firmware (v4.x.x, circa 2015) revealed a critical generational architecture change. Solarflare transitioned from a static flash memory partition to a dynamic memory layout in later firmware versions.
* **Incompatibility:** Attempting to install modern drivers or use modern `sfupdate` utilities on the legacy static partition resulted in silent failures, as the utilities were incompatible with the older memory schema.

## Diagnostic Path & MC Corruption

* **Interrupt Hooking Interference:** Initial flashing attempts utilizing Ventoy bootable USBs caused unpredictable behavior. It was determined that Ventoy's interrupt hooking interfered with the low-level block device access required by the flashing utilities.
* **Legacy Environment Emulation:** Reverted to a native, legacy TinyCore environment utilizing a 2015 release of `sfutils` to match the native firmware epoch.
* **MC (Management Controller) Lockout:** Repeated cross-version flashing attempts triggered a hardware-level safety mechanism. The Management Controller (MC) ceased pushing new firmware to the ROM, effectively locking the volatile memory into a read-only protective state to prevent a total brick.
* **Conclusion:** The card is locked into the legacy 2015 firmware, preventing the use of modern OpenOnload drivers on modern Linux kernels.

## PCIe Topology & Bifurcation Optimization

* **Bandwidth Throttling:** Initial `lspci` diagnostics indicated the SN7122F was only receiving 4 PCIe lanes, despite being seated in a physical x16 slot.
* **Bifurcation Analysis:** Investigation into the motherboard's architecture revealed that the secondary x16 slot is electrically wired as x4. Furthermore, these lanes are routed through the PCH (Platform Controller Hub / "The Bridge") rather than directly to the CPU, introducing unacceptable latency overhead for a high-frequency trading application.
* **Resolution:** Re-architected the physical layout by migrating the GPU to the PCH-routed slot. The SN7122F was seated in the primary PCIe slot, securing direct, unmediated access to the CPU lanes to minimize hardware-level latency.

## Architectural Progression: Pushing the Standard Stack

While the OEM firmware lock currently prevents true kernel bypass via `ef_vi`, this hardware limitation perfectly aligned with my broader architectural learning progression.

* **First-Principles Philosophy:** Jumping straight into kernel bypass abstracts away the fundamental realities of the operating system. To truly understand the "why" behind low-latency networking, I deliberately needed to understand why is it so much faster. I want to fully hit the bottlenecks of the standard Linux network stack before I earn the right to bypass it.
* **Pushing the POSIX Limit:** Because of this, the engine MVP was intentionally designed to extract performance out of the standard kernel stack where it was easy, but also leave room to appreciate and reiterate on better, more data oriented design. This drove the implementation of the highly optimized `epoll` asynchronous event loop and strict non-blocking socket architectures utilizing `EPOLLET` (edge-triggering).
* **Establishing the Baseline:** By building this foundation first, I now have an exact, profiled understanding of syscall overhead, context-switching penalties, and queue starvation. When I eventually unlock the SN7122F MC or acquire unlocked hardware, the transition to zero-copy won't just be blindly using an API—it will be a deeply understood, measurable architectural leap.