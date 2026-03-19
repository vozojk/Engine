# Log 005: Lock-Free Logging & Paged Array Architecture

**Date:** 2026-03-19  
**Focus:** Lock-Free Data Structures, Memory Ordering, Bitwise Operations, Flat Architecture

Today’s focus was on decoupling I/O from the critical path by implementing an asynchronous logger, alongside finalizing the architectural blueprint to replace all dynamic allocations (Hashmaps and RBTs) in the Order Book with flat, cache-friendly data structures.

## Table of Contents
1. [Lock-Free Ring Buffer Logger](#lock-free-ring-buffer-logger)
2. [Hardware Memory Models (x86 vs. ARM)](#hardware-memory-models-x86-vs-arm)
3. [Order Book Architecture: Paged Arrays & Flat Hash Maps](#order-book-architecture-paged-arrays--flat-hash-maps)

## Lock-Free Ring Buffer Logger

Implemented a dedicated logger class to write off data to a buffer, which a separate consumer thread reads and prints in a loop, completely removing I/O stalls from the hot path.

* **Header-Only Inlining:**
    * **Design:** The `Logger` class (including `start()`, `stop()`, `consumer()`, and `log()` functions) and the `LogEntry` struct (char array message + `rdtsc()` timestamp) are implemented entirely within a header file.
    * **Rationale:** Forces the compiler to inline the hot-path logging calls directly into the execution flow, eliminating stack frame overhead.
* **Bitwise Modulo (Zero-Overhead Wrapping):**
    * **Implementation:** The ring buffer uses a templated capacity that must be a power of 2, resolving at compile time with zero overhead.
    * **Mechanism:** By using `capacity - 1`, we create a bitmask setting all bits except the MSB to 1. The buffer wraps the pointer using a bitwise AND (`&`) against this mask.
    * **Result:** Simulates a modulo operation but bypasses expensive CPU integer division, reducing wrapping logic to a single bitwise instruction.
* **Atomic Memory Ordering:**
    * **Implementation:** Encapsulated state (buffer, running state, head/tail pointers) is managed using `std::atomic` with explicit `memory_order_acquire`, `memory_order_release`, and `memory_order_relaxed` semantics.
    * **Impact:** Enables lock-free execution by design. Properly flushes the CPU's store buffer and invalidation queues without utilizing heavy OS-level mutexes.

## Hardware Memory Models (x86 vs. ARM)

* **Architectural Insight:** The atomic memory orderings highlight a critical difference in CPU instruction pipelines:
    * **x86 (Strongly Ordered):** x86 CPUs utilize a strict Total Store Order (TSO) system. Instruction reordering is heavily restricted by the silicon itself, meaning our `memory_order` calls function primarily as compiler directives to prevent C++ instruction reordering. This is why atomics operate with such low overhead on x86.
    * **ARM / Apple Silicon (Weakly Ordered):** These architectures execute very aggressive hardware-level instruction reordering. If deployed on these chips, the aforementioned `memory_order` directives compile down into actual physical barrier instructions. Without them, the lock-free logic would fail almost immediately.

## Order Book Architecture: Paged Arrays & Flat Hash Maps

Finalized the architectural research to flatten the current data structures. We will strip out `std::map` (RBTs) and standard Hashmaps to resolve memory constraints and maximize L1 cache efficiency.

* **The Global Page Pool (Replacing RBTs):**
    * **Strategy:** Pre-allocate a global pool of tens of thousands of `PricePage` structs. Each page covers a specific price range (e.g., $10) and contains pre-allocated `PriceLevel` structs.
    * **Mechanism:** These pages are not mapped to specific stocks at startup. Instead, pointers to these pages will be dynamically handed out on demand to individual locate Order Books strictly based on active price action.
* **The Master Directory (Locate-Specific Routing):**
    * **Implementation:** Each stock (locate) will maintain a local directory—an array of roughly a thousand pointers, each representing one chunk of the price range (e.g., `[0, pagesize * 1000]`).
    * **Logic:** These pointers default to `nullptr`. When a stock trades into a new price range, the directory stores the pointer provided by the Global Page Pool, ensuring contiguous $O(1)$ price lookups without dynamic tree allocations.
* **Custom Flat Hash Pool (Replacing unordered_map):**
    * **Strategy:** The actual orders will be moved out of `std::unordered_map` and into a custom hash pool.
    * **Mechanism:** This structure will use a custom hash function specifically fitted to the size of the underlying contiguous array, utilizing Linear Probing to handle collisions. This ensures all order lookups strictly traverse contiguous memory, leveraging the hardware prefetcher and avoiding pointer-chasing heap lookups.