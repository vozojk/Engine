# Log 003: Network Stack Integration & Event-Driven Architecture

**Date:** 2026-03-04  
**Focus:** Zero-Copy I/O, Kernel Networking, Asynchronous Polling, System Architecture

Today’s focus was transitioning the engine from a static, batch-processing file parser into a live, event-driven network system. I successfully implemented the UDP infrastructure, built a simulated exchange to stress-test the pipeline, and restructured the entire repository to align with production-grade architectural standards.

## Table of Contents
1. [Repository Architecture & CMake Restructuring](#repository-architecture--cmake-restructuring)
2. [The Mock Exchange & Zero-Copy mmap Cannon](#the-mock-exchange--zero-copy-mmap-cannon)
3. [Event-Driven UDP Receiver (epoll)](#event-driven-udp-receiver-epoll)
4. [Parser Streamlining & Telemetry Migration](#parser-streamlining--telemetry-migration)

## Repository Architecture & CMake Restructuring

To support the addition of networking components and maintain a clean separation of concerns, the repository was completely restructured.

* **Component Decoupling:**
    * **Strategy:** Separated the core `engine` and the new `mock_exchange` into distinct directories with independent executables.
    * **Mechanism:** Introduced a nested CMake architecture. A root `CMakeLists.txt` manages the build, while individual CMake files handle the specific targets.
    * **Result:** Clean dependency trees and modular compilation. Added a `commons` folder to centralize struct definitions and shared utilities across both applications.
* **Build Profiles:**
    * **Implementation:** Added a `CMakePresets.json` file defining strict `Debug` and `Release` options.
    * **Rationale:** Eliminates manual flag configuration during compilation, ensuring benchmarking runs are strictly executed under optimal release `-O3` conditions without having to manually load them every run.
* **Documentation Philosophy:**
    * **Decision:** Split documentation into `devlogs/` (formal architectural decisions like this one) and `braindumps/`.
    * **Rationale:** `braindumps/` will store unfiltered, unformatted deep-dives into kernel mechanics and hardware operations exactly as I understand them in the moment. This preserves the raw, first-principles thinking process before it gets sanitized into formal documentation.

## The Mock Exchange & Zero-Copy mmap Cannon

Created `mock_exchange/main.cpp` to simulate a live exchange firing UDP packets. The primary architectural challenge was loading a massive historical `market.bin` file from the resources folder without introducing I/O bottlenecks that would artificially throttle the sender.

* **Zero-Copy File Ingestion (`mmap`):**
    * **Strategy:** Bypassed the standard `read()` system call and the Kernel Page Cache entirely using `mmap`.
    * **Mechanism:** Memory-mapping maps the file directly into the user-space virtual memory. We only physically copy data into RAM when the CPU actually accesses the pointer, bypassing the kernel page cache and user-space copy.
    * **Challenge:** Fetching unmapped data requires a ~50 microsecond hardware IRQ to the SSD to fetch that data.
    * **Solution:** The Linux kernel is smart and realizes it will need the sequential data, pre-copying it and effectively saving us the extra copies to the kernel page cache with a very low downside.
* **Network Stack Constraints & Loopback Shortcut:**
    * **Observation:** The Cannon currently "sends" data at ~2.5 microseconds per message.
    * **Insight:** This speed is artificial because it is not actually traversing the physical wire. By looping back, it bypasses a majority of the network stack it is supposed to go through (a true network stack traversal would cost ~8-10 microseconds).
    * **The Hairpin Fight:** I disabled the checksum for the packets and tried to use the `eth0` WSL IP, thinking it could help with the loopback problem and force a real network traversal, but it did not. The OS routing table is too smart and hairpins it anyway.

## Event-Driven UDP Receiver (epoll)

Replaced the Engine's static file-reading loop with a live socket listener capable of reacting to asynchronous network traffic.

* **Asynchronous Polling:**
    * **Implementation:** Added a UDP receiver to the main of the engine. This binds and listens to the port of the exchange sender, and with the use of `epoll`, scans and waits for packets to arrive.
    * **Rationale:** Unlike older POSIX `select` or `poll` functions which suffer from an $O(N)$ linear scan bottleneck, `epoll` uses a kernel-level Red-Black tree and a Ready List to achieve $O(1)$ scaling. It efficiently monitors the socket buffer and wakes the thread to immediately trigger the ITCH parser function only when actual network traffic is received.

## Parser Streamlining & Telemetry Migration

* **Hot-Path Cleanup (`parser.hpp`):**
    * **Refactoring:** Removed unnecessary comments, debugging logging lines, and the static file `while` loop, since the parser now gets called strictly per-packet.
    * **Result:** The parser is now a pure, stateless function that executes cleanly on incoming network data.
* **Telemetry Relocation:**
    * **Decision:** Moved the nanosecond telemetry logic out of the parser and into the `epoll` `while` loop in `main.cpp`.
    * **Impact:** Provides an accurate measurement of total latency per packet batch directly at the network ingestion point.