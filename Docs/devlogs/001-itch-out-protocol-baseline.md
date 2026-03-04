# Log 001: Initial Architecture
## The Data Structure
**Date:** 2026-02-14\
**Focus:** Correctness, Memory Layout, Endianness

The intial architecture at this point spans a few major architectural decisions. I will describe them here as a baseline for this project.
## Table of Contents
1. [ITCH/OUCH Message struct definitions](#itch/ouch-message-struct-definitions)
2. [ITCH Parsing logic](#itch-parsing-logic)
3. [First benchmarking logic and measurements](#first-benchmark)
4. [Data structure choice and creation (The Order Book array)](#data-structure-choice-and-creation-the-order-book-array)

## ITCH/OUCH Message struct definitions

The engine implements a zero-copy ingestion pipeline for the NASDAQ ITCH 5.0 protocol. The data model is designed to minimize serialization overhead and maximize cache locality.

### Architectural Decisions
* **Zero-Copy Parsing:** * **Strategy:** Messages are parsed via `reinterpret_cast` directly over the UDP receive buffer.
    * **Optimization:** Use pack(push, 1) to eliminate compiler padding ensuring memory layout maps to incoming data.
    * **Result:** Elimination of `memcpy` cycles during the hot path.

* **Endianness Safety:**
    * **Constraint:** ITCH is Big-Endian (Network Byte Order); x86_64 is Little-Endian.
    * **Solution:** Raw casting provides the structure, but fields get swapped with bswap to ensure CPU can read the data.

* **Type Semantics:**
    * **Usage:** Strictly `unsigned` integers.
    * **Reasoning:** Aligns with protocol definitions (prices are positive) and guarantees defined behavior for overflow/wrapping logic.
## ITCH Parsing logic

Started the ITCH 5.0 parser. The primary goal was to process packets without triggering unnecessary `memcpy` operations that pollute the cache forcing for slow copies from the RAM.

* **Stream Processing (Pointer Arithmetic):**
    * The ingestion buffer (currently memory-mapped binary; extensible to NIC ring buffers) is treated as a contiguous byte stream.
    * **Mechanism:** A "moving window" cursor (`const char*`) advances through the buffer.
    * **Dispatch:** A single-byte lookahead reads the `Message Type` to trigger the switch-case dispatcher before the full payload is processed.

* **Zero-Copy Type Punning:**
    * **Technique:** We utilize `reinterpret_cast<const MsgStruct*>` to overlay the packed struct definition directly onto the raw buffer address.
    * **Optimization:** This avoids `memcpy`. The "parse" is purely a compile-time directive telling the CPU how to interpret the bits at the current address.
    * **Constraint:** Requires `pack(push, 1)` to ensure struct alignment matches the ITCH wire protocol exactly.



* **48-bit Timestamp Recomposition:**
    * **Challenge:** ITCH timestamps are 6 bytes (48-bit) and Big-Endian (Network Byte Order), which native x86_64 registers cannot load atomically.
    * **Solution:**
        1.  Load 8 bytes (64-bit) from the offset.
        2.  Apply `__builtin_bswap64` to correct Endianness.
        3.  Right-shift and Mask to isolate the 48-bit nanosecond value.
    * **Result:** Single-register operation rather than multiple memory fetches.

* **Strict Copy-Avoidance:**
    * Data is passed downstream via `const` pointers. Values are only copied to the stack when mutation is required (e.g., updating an Order Book level), ensuring the L1/L2 cache remains filled only with relevant hot data.
* **Note:**
  * The parsing logic will later be supplemented with checks to read only structs for which we have all the data (check end pointer to length+start) to avoid segfaults. This will also allow to read 64 bits for the timestamp making it faster. (mov 64 bit mask + swap instead of mov 16 bit, mov 32 bit, swap, the former is much faster since the CPU always moves 64 bits and masking costs basically nothing while sub-64-bit moves often require the CPU to "merge" the data into existing registers, which can cause partial register stalls. and we have to do 2 of them).
* **Header-Only Inlining Strategy:**
    * **Decision:** The entire parsing and dispatch logic is implemented within header files using the `inline` keyword.
    * **Rationale:** This ensures the compiler can perform **Instruction Inlining**, effectively copy-pasting the parsing logic directly into the main event loop.
    * **Impact:** Eliminates the overhead of function call stack frames and branch penalties associated with jumping to external object symbols, keeping the instruction pipeline "warm" and predictable.
## First benchmarking logic and measurements
* **Current State (The "Chrono" Baseline):**
  * **Tooling:** Utilizing `std::chrono::high_resolution_clock`.
  * **Issue:** Initial tests show significant jitter and "Measurement Overhead." The clock might be introducing more latency than the logic it is trying to measure.

* **The Road to High-Resolution:**
  * **Observation:** To get a "Clean" read, I need to move from wall-clock time to **CPU Cycles**.
  * **Next Step:** Implement a `rdtscp` (Read Time Stamp Counter) wrapper to bypass the OS and get cycle-accurate data directly from the silicon.
  * **Goal:** Once the hardware timer is in place, I will begin **Core Isolation** to eliminate OS-level interrupts that are currently spiking the tail latency.
## Data structure choice and creation (The Order Book array)

Achieve $O(1)$ order manipulation and $O(\log N)$ price discovery using a hybrid data structure.

### 1. The Hybrid Model
* **Price Tracking (RB-Trees):** Utilized `std::map` for Bids (sorted `std::greater`) and Asks.
  * **Logic:** This ensures the inside market (Best Bid/Offer) is always at `begin()`. It provides $O(\log N)$ insertion for new price levels and automatic sorting for depth.
* **Order Tracking (The "Shortcut"):** Implemented `std::unordered_map<OrderID, std::list<Order>::iterator>`.
  * **Rationale:** Finding an order in a tree or list by ID is $O(N)$ or $O(\log N)$. By storing iterators in a hashmap, we achieve $O(1)$ "jumps" to any order in the book for cancels or executions.



### 2. Architectural Decisions & Micro-Optimizations

* **The Parent Pointer Strategy:**
  * **Challenge:** An iterator gives us the `Order`, but to update the `PriceLevel` (e.g., subtracting volume), we’d normally have to re-search the tree.
  * **Solution:** Each `Order` struct holds a pointer to its `Parent` (PriceLevel).
  * **Impact:** We can modify the price level's total volume and delete the order in one consolidated step. Total elimination of redundant tree traversals.

* **Branch Prediction & Determinism:**
  * **Decision:** Explicitly separated `deleteOrder` and `reduceOrderSize`.
  * **Insight:** Even though the logic is similar, separating them creates distinct instruction paths. This assists the **CPU Branch Predictor** in learning the specific patterns of "Full Cancels" vs. "Partial Fills," reducing pipeline stalls during high-volatility bursts.

* **Register Promotion via Local Variables:**
  * **Logic:** Instead of repeatedly dereferencing `plList->TotalVolume`, I pull hot values into local variables.
  * **Reasoning:** This signals the compiler to keep these values in **General Purpose Registers**. It minimizes "Pointer Aliasing" issues where the CPU is afraid to cache a value because it thinks another pointer might change it.

* **Struct Alignment (The "Big to Small" Rule):**
  * **Implementation:** Attributes in the `Order` struct are sorted by size (64-bit -> 32-bit -> 16-bit).
  * **Result:** Maximizes memory density by eliminating compiler-inserted padding. This ensures more orders fit into a single **64-byte Cache Line**.

---
**Current Technical Debt:**
* **The Heap:** `std::map` and `std::list` are node-based and hit the heap for every new order.
* **Next Step:** Moving toward **Memory Pools** to pre-allocate order nodes and eliminate `new/delete` jitter on the hot path.
