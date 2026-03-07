# Log 004: Network Stack Finalization & Syscall Avoidance MVP

**Date:** 2026-03-06  
**Focus:** Bidirectional TCP, Deterministic Latency, Stream Coalescing, Syscall Avoidance

Today marks the completion of the Engine MVP. The system is now a fully functional, bidirectional, event-driven architecture capable of ingesting an ITCH UDP firehose while concurrently managing stateful OUCH TCP order executions. Major architectural upgrades were implemented to avoid kernel-level context switching and maximize wire-level throughput.

## Table of Contents
1. [Bidirectional OUCH & ITCH Integration](#bidirectional-ouch--itch-integration)
2. [Syscall Avoidance & Deterministic Timing (RDTSC)](#syscall-avoidance--deterministic-timing-rdtsc)
3. [Socket Optimization & Flow Control](#socket-optimization--flow-control)
4. [Stream Coalescing & Vectorized I/O (Bundling)](#stream-coalescing--vectorized-io-bundling)
5. [Telemetry & State Management](#telemetry--state-management)

## Bidirectional OUCH & ITCH Integration

The engine now successfully multiplexes two entirely different network protocols within a single event loop.

* **Multithreaded Mock Exchange:**
  * **Implementation:** The Exchange simulator utilizes `std::thread` to strictly separate the UDP ITCH sender and the TCP OUCH responder onto different CPU cores.
  * **Rationale:** Prevents the massive I/O load of the ITCH firehose from stalling the exchange's ability to process and acknowledge incoming OUCH orders, maximizing overall systemic throughput.
* **Startup Synchronization:**
  * **Mechanism:** Implemented a strict 10-second initialization wait-state on the exchange side.
  * **Impact:** Ensures the Engine has sufficient time to complete the TCP 3-way handshake and initialize its `epoll` tree before the UDP data stream begins.

## Syscall Avoidance & Deterministic Timing (RDTSC)



Replaced all OS-level sleep calls with a custom hardware-level spin-loop to simulate exact market conditions and push the engine to its processing floor.

* **The `usleep()` Syscall Trap:**
  * **Observation:** Utilizing `usleep(X)` for intra-packet delays introduced massive latency jitter (often 2.5µs+ baseline).
  * **Insight:** `usleep(0)` is a `sched_yield` system call. The CPU context-switches to Kernel Space, penalizing the thread regardless of the inputted sleep duration. This lack of resolution makes high-frequency simulation impossible.
* **The Spin-Loop Solution (`__rdtsc`):**
  * **Implementation:** Replaced the syscall with a user-space busy-wait loop reading the Time-Stamp Counter (`RDTSC`), tracking literal CPU clock cycles for nanosecond precision.
  * **Optimization (`_mm_pause`):** Injected the `_mm_pause()` intrinsic into the loop. This informs the CPU that it is spinning, preventing aggressive speculative execution. This keeps the core hot, prevents thermal throttling, and eliminates the massive pipeline flush penalty when the loop condition breaks.
  * **Impact:** Allows the exchange to precisely throttle the feed, pushing the engine right to its current parsing limit of roughly **~10ns per message**.

## Socket Optimization & Flow Control



Refactored both UDP and TCP file descriptors to operate purely asynchronously, placing the flow-control burden strictly on the application layer.

* **Defeating Nagle's Algorithm:**
  * **Implementation:** Applied the `TCP_NODELAY` socket option to the OUCH connection.
  * **Rationale:** Nagle's algorithm artificially delays sending packets to buffer more data, which is catastrophic for HFT Tick-to-Trade latency. Disabling it ensures orders are flushed to the NIC instantly.
* **Non-Blocking I/O & Edge-Triggering (`EPOLLET`):**
  * **Implementation:** Both sockets are set to non-blocking (`O_NONBLOCK`). The engine reads until the kernel throws `EAGAIN` or `EWOULDBLOCK`, using this flag to dictate when to parse the buffer or move to the next socket.
  * **Starvation Prevention:** A 500k msg/sec UDP stream will never trigger `EAGAIN`, permanently trapping the thread and starving the TCP socket. Implemented a **Read Quota** (batch limit) to force the `epoll` loop to yield and process order ACKs, ensuring socket fairness.
* **Handshake Lifecycle Management (`EPOLLOUT`):**
  * **Mechanism:** The TCP socket is initially registered with `EPOLLOUT` to asynchronously finalize the non-blocking `connect()` handshake. Once connected, `EPOLLOUT` is dynamically unregistered via `epoll_ctl` to prevent the event loop from infinitely spinning when the out-buffer is writable.

## Stream Coalescing & Vectorized I/O (Bundling)



Optimized the ingestion pipeline to handle high-density data bursts and raw byte-stream continuous data.

* **Exchange-Side Bundling:**
  * **Strategy:** The exchange now batches multiple ITCH/OUCH messages into a single large UDP datagram/TCP payload before sending.
  * **Result:** By paying the header/packaging tax once per bundle rather than once per message, throughput scaled from ~400k msgs/sec to the multi-millions. At these speeds, the engine hits a bottleneck at around **~10ns per message**, where CPU memory operations (copying/struct mapping) overtake network I/O as the primary constraint.
* **Engine-Side Debundling (Pointer Arithmetic):**
  * **Challenge:** TCP is a continuous byte stream, not a discrete message protocol. The OS Kernel or NIC will coalesce multiple OUCH ACKs into a single buffer array.
  * **Solution:** Implemented a strict while-loop scanning the receive buffer using byte-offset pointer arithmetic (`buffer + processed`). The parser reads the header size, casts the struct, and slides the pointer forward until the buffer is exhausted, preventing dropped packets during microbursts.

## Telemetry & State Management

* **Out-of-Path Connection Logging:** Extensive state-logging occurs during the TCP handshake phase. Because this is isolated to the initialization sequence, it carries zero penalty during the hot-path execution.
* **Hardware-Level Load Shedding:** Active `std::cout` telemetry during the hot path is strictly filtered to a single instrument (AAPL). This updates the logged price from the order book without stalling the CPU with excessive terminal I/O.
* **Order Validation:** The engine successfully parses and logs Order Accepted (`ACK`) messages from the OUCH feed, and the exchange actively echoes these ACKs to the terminal to monitor successful bid/ask executions.