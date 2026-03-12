# Engine

A low-latency trading engine built from scratch in C++20 that ingests a full NASDAQ ITCH 5.0 market data firehose over UDP while simultaneously managing bidirectional OUCH order sessions over TCP. The entire system is event-driven, zero-copy where it matters, and obsessively tuned around one principle: **don't let the kernel touch your hot path.**



Current simulated exchange UDP speed sits at roughly **~10ns per message** at high MTU on a single core.



## What This Actually Does

The engine multiplexes two completely different network protocols inside a single `epoll` event loop:

1. **ITCH 5.0 (UDP Inbound)** — The full market data feed. Every order add, cancel, execute, delete and replace on the exchange gets parsed and applied to a per-stock order book in real time.

2. **OUCH (TCP Bidirectional)** — The order entry protocol. When the engine spots something it likes in the ITCH stream, it fires an `EnterOrder` over TCP to the exchange and tracks the full lifecycle (Accepted → Partially Filled → Filled / Cancelled / Broken / Rejected) through the response stream.



A bundled mock exchange simulates both sides — blasting historical market data over UDP while running a TCP responder that ACKs incoming orders. The two exchange threads run on separate cores to prevent the ITCH firehose from stalling OUCH acknowledgments.



## Architecture

```
Engine/
├── trading_engine/          # The core engine
│   ├── main.cpp             # Event loop, epoll, socket init
│   ├── Parser.hpp           # ITCH 5.0 zero-copy parser (header-only for inlining)
│   ├── OUCH_Parser.hpp      # OUCH order lifecycle manager
│   ├── Order_Book.cpp/hpp   # Per-stock limit order book
├── mock_exchange/           # Simulated exchange for stress testing
│   └── main.cpp             # UDP cannon + TCP responder (multithreaded)
├── common/                  # Shared struct definitions
│   ├── ITCH_Messages.hpp    # All ITCH 5.0 message structs (pack(push,1))
│   ├── OUCH_Messages.hpp    # OUCH message structs + order state machine
│   └── Types.hpp            # Order, PriceLevel — cache-friendly layout
├── Docs/
│   ├── devlogs/             # Formal architectural decision records
│   └── braindumps/          # Raw, unfiltered deep-dives into kernel internals
├── CMakeLists.txt           # Root build — C++20, -O3, -march=native, LTO
└── CMakePresets.json        # Debug/Release profiles
```

## The Interesting Parts



### Zero-Copy Parsing



Messages arrive as raw bytes from the network. Instead of deserializing them into objects, I overlay packed structs directly onto the receive buffer using `reinterpret_cast`. The "parse" is a compile-time directive — the CPU just gets told how to interpret the bits at a given address. No `memcpy`, no allocation, no cache pollution.



The constraint is that struct memory layout has to match the wire protocol exactly. I enforce this with `#pragma pack(push, 1)` to kill compiler padding and `static_assert` to verify sizes at compile time. Fields that arrive Big-Endian get swapped with `__builtin_bswap` intrinsics. The 48-bit ITCH timestamps get loaded as 8 bytes, byte-swapped, then masked — single-register operation.



### The Order Book



Each of the 65,536 possible stock locates gets its own `OrderBook`. Internally:

- **Price levels** live in `std::map` (Red-Black Tree) — sorted by price, `std::greater` for bids so `begin()` is always best bid

- **Orders within a level** sit in `std::list` — FIFO for time priority, O(1) insert/remove

- **O(1) order lookup** via `std::unordered_map<OrderID, list::iterator>` — every modify/cancel/execute is a direct iterator dereference, no searching



Struct fields are ordered largest-to-smallest to eliminate padding. The parent `PriceLevel*` pointer lives inside each `Order` so we never have to walk back up the tree.



### Syscall Avoidance



The single biggest realization from this project: **context switches are the enemy.** Every `usleep()`, every `printf()`, every avoidable trip to kernel space costs microseconds you can't get back. Specifically:



- `usleep(0)` is not free — it's a `sched_yield` syscall. The CPU switches to ring 0 regardless of the argument. I measured ~2.5µs overhead *minimum*.

- **Solution:** A user-space spin-loop using `__rdtsc()` (reads the literal CPU cycle counter) with `_mm_pause()` injected to prevent speculative execution from building up a pipeline that has to be flushed when the loop breaks.

- `epoll` runs in **edge-triggered** mode (`EPOLLET`) — the kernel notifies us once when new data arrives, not continuously. More responsibility, fewer syscalls.

- Nagle's algorithm is disabled (`TCP_NODELAY`) — we're sending small OUCH messages and need them on the wire immediately, not buffered.
  

### The Mock Exchange

The exchange uses `mmap` to map a ~8GB historical `market.bin` file directly into virtual memory, bypassing `read()` and the kernel page cache entirely. The kernel is smart enough to prefetch sequential pages, so we get near-zero I/O overhead.

Messages get bundled up to the MTU limit (~1400 bytes) before hitting `sendto()`, amortizing the syscall cost across ~40 messages per packet. The engine side unpacks the bundle by reading the 2-byte length prefix of each message and advancing through the buffer.

### OUCH Order Management

Outbound orders get tracked in a preallocated array of 1M `MyOrder` structs — indexed by `UserRefNum` for instant O(1) access. Each order has a full state machine (`PENDING_NEW → LIVE → PARTIALLY_FILLED → FILLED / CANCELLED / REJECTED`). Broken trades get reconciled through a separate `fillTracker` keyed by match number so the main book doesn't get bloated.

## Prerequisites: The Market Data

This engine requires a binary NASDAQ ITCH 5.0 data file to simulate the exchange firehose. Because real tick data files are massive (often several gigabytes), `market.bin` is not included in this repository. 

To run the engine, you must:
1. Obtain an ITCH 5.0 binary file (e.g., from NASDAQ's historical data FTP, https://emi.nasdaq.com/ITCH/Nasdaq%20ITCH/).
2. Rename it to `market.bin`.
3. Place it directly in the `/mock_exchange` execution directory and update the file path in `mock_exchange/main.cpp` on line 224.

## Building

```bash
# Release (the only way that matters for benchmarking)
cmake --preset release
cmake --build build/release

# Debug (for when things inevitably break)
cmake --preset debug
cmake --build build/debug
```

Requires: Linux, C++20, CMake 3.20+. The build enables `-O3 -march=native` and Link-Time Optimization (LTO) in release mode — LTO lets the compiler inline across translation units, effectively creating one giant optimized binary.

## Running

```bash
# Terminal 1 — Start the exchange (arg = nanoseconds between packets)
./build/release/mock_exchange/exchange 5000

# Terminal 2 — Start the engine
./build/release/trading_engine/engine
```

The exchange waits 10 seconds before firing to give the engine time to complete the TCP handshake and initialize `epoll`. Telemetry prints every 100k messages with throughput (msgs/sec) and average latency (ns/msg).

## Documentation

The `Docs/` folder has two types of documentation for two very different purposes:

- **`devlogs/`** — Formal architectural records. Every major decision, data structure choice, and optimization with Strategy/Mechanism/Result breakdowns. Start with [001](Docs/devlogs/001-itch-out-protocol-baseline.md) and read sequentially.
- **`braindumps/`** — Raw, unfiltered, unformatted deep-dives written exactly as I was learning. The full Linux RX path from NIC hardware interrupt to `epoll` wake-up. How TCP connections actually get established at the kernel level. Why `usleep` destroys latency. These preserve the first-principles thinking before it gets cleaned up.

## What's Next

- **Kernel bypass (DPDK/ef_vi)** — Skip the kernel network stack entirely. Map NIC ring buffers directly into user space. This is the real endgame for latency.
- **Asynchronous logging** — Replace `cout` with a lock-free ring buffer drained by a dedicated I/O thread. Right now every print is a syscall that stalls the hot path.
- **Read quota on the UDP loop** — The edge-triggered `epoll` loop currently drains the entire UDP buffer before checking TCP. Under heavy load this starves OUCH responses. Need to cap reads per wake-up.
- **Risk management layer** — Position tracking by stock, exposure limits, and PnL.
- **Strategy layer** — The current "buy AAPL on any add order" is a placeholder. Real signal generation goes here.

## Stack

- **Language:** C++20
- **Build:** CMake 3.20+ with presets
- **Networking:** Raw POSIX sockets, `epoll` (edge-triggered), `TCP_NODELAY`
- **Protocols:** NASDAQ ITCH 5.0 (UDP), NASDAQ OUCH (TCP)
- **Optimization:** `-O3`, `-march=native`, LTO, `__rdtsc`, `_mm_pause`, `mmap`, zero-copy `reinterpret_cast`
- **Platform:** Linux (x86_64)
