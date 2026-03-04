# Log 002: Finishing OUCH Parsing MVP

**Date:** 2026-02-27
**Focus:** Functionality, Upgradability, Baseline Speed, Compatibility

Today marks the finish of the entire minimum viable product (MVP) parsing structure for the engine. From this point forward, the focus shifts strictly to upgrades and connecting with other system components. The core additions established today solidify our order tracking and memory efficiency.

## Table of Contents
1. [Struct Memory Layout Optimization](#struct-memory-layout-optimization)
2. [Execution Book & Trade Tracking](#execution-book--trade-tracking)
3. [High-Speed Outbound Order Book](#high-speed-outbound-order-book)

## Struct Memory Layout Optimization

* **Cache-Friendly Struct Ordering:**
    * **Strategy:** Reordered all eligible (only the ones I am creating) fields within the OUCH structs to descend in size (e.g., 64-bit variables first, followed by 32-bit, 16-bit, and 8-bit).
    * **Mechanism:** The compiler naturally inserts invisible padding bytes to ensure data types align to their required memory sizes (e.g. an 8-byte integer will need to have an address divisible by 8).
    * **Result:** By sorting from largest to smallest, we inherently satisfy these boundary requirements without the compiler needing to inject padding. This maximizes data density, ensuring more relevant data fits cleanly into a single CPU cache line.

## Execution Book & Trade Tracking

Established an Execution Book to independently track partial and full executions utilizing the exchange-provided Match Number.

* **Execution Tracking:**
    * **Implementation:** Created a dedicated struct to save the executed quantity, price, and match number.
    * **Rationale:** Including the Match Number in the struct ensures it is self-aware and can be easily passed through the system. The price is retained for additional bookkeeping. This is critical for possible future Risk Management implementation.
* **Handling Broken Trades:**
    * **Challenge:** Broken Order messages do not contain our internal `userRefNum`; they only reference the specific transaction via Match Number.
    * **Solution:** We index the Execution Book by Match Number. When a bust occurs, we locate the specific execution and retrieve the share quantity involved. We then deduct that amount from **both** the `Total Shares Requested` and the `Filled Shares` in our high-speed book. Because the broken portion of the order is considered dead, and we do not automatically send a new one, the outstanding order only has the remaining shares left, effectively decreasing the total order size.

## High-Speed Outbound Order Book

Completed the data structure required to track our outbound OUCH orders in a separate, highly optimized access book.

* **State Management (Enum Class):**
    * **Design:** Utilized an `enum class` to strictly define and track the lifecycle state of every outbound order (e.g., Open, Filled, Cancelled, Broken).
    * **Impact:** Allows for instantaneous status checks, easily keeping track of all open orders and our current holdings.
* **Lifecycle Mathematics:**
    * **Fields:** The struct explicitly tracks `Total Shares Requested`, `Filled Shares`, and `Open Shares`.
    * **Logic:** Open shares are dynamically calculated (Total - Filled - Cancelled). These values, alongside the state enum, are updated based on the stream of incoming OUCH messages from the exchange.
* **Extensibility:** This baseline can easily be extended into a broader global book tracking positions by `stockLocate` and price, mirroring the ITCH global book mechanics.