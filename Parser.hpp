//
// Created by vojte on 12/02/2026.
//

//apparently, if i put this in a header even though headers are for definitions
//the compiler can inline the code into where its called basically creating one huge file
//which increases performance since no other file has to be accessed

#ifndef THE_THING_PARSER_HPP
#define THE_THING_PARSER_HPP

#include <iostream>
#include <fstream>
#include "ITCH_Messages.hpp"
#include "Order_Book.hpp"

#include <cstdint>
#include <vector>
#include <chrono> // <--- THE HEADER
using namespace std::chrono;
using namespace std;

// A Lookup Table: Index = Locate ID, Value = Stock Symbol
// 65536 is enough for all US stocks (max is usually ~12,000)
namespace ITCHParser {
    std::vector<std::string> stock_directory(65536);
    int counter = 0;

    // 1. Swap 16-bit (2 bytes) - used for Lengths and Message Types
    //the inline is gray but its to signal
    //that the function should be inlined
    static inline uint16_t bswap16(uint16_t x) {
        return __builtin_bswap16(x);
    }

    // 2. Swap 32-bit (4 bytes) - used for Prices and Shares
    static inline uint32_t bswap32(uint32_t x) {
        return __builtin_bswap32(x);
    }

    // 3. Swap 64-bit (8 bytes) - used for Order IDs and Timestamps
    static inline uint64_t bswap64(uint64_t x) {
        return __builtin_bswap64(x);
    }

    //get timestamp into a 64 bit number
    static inline uint64_t parseTimestamp(uint8_t* x) {
        return ((uint64_t)x[0] << 40 |
                (uint64_t)x[1] << 32 |
                (uint64_t)x[2] << 24 |
                (uint64_t)x[3] << 16 |
                (uint64_t)x[4] << 8 |
                (uint64_t)x[5]);
    }

    // TIP To <b>Run</b> code, press <shortcut actionId="Run"/> or click the <icon src="AllIcons.Actions.Execute"/> icon in the gutter.
    inline void parse(char* ptr, char* end, OrderBook* book) {

        char* start = ptr;
        //no check for size so far (file is 2gb)

        std::cout << "ENGINE STARTING!" << std::endl;
        auto start_time = high_resolution_clock::now(); //start clock
        while (ptr < end) { //switch to while (file) for full file, checks for end of file
            counter++;
            MessageHeader* headerPtr = reinterpret_cast<MessageHeader*>(ptr);
            ptr += 2;

            //file.read(reinterpret_cast<char*>(&header), sizeof(header)); //read the header part and create the object

            uint16_t length = bswap16(headerPtr -> Length);

            switch (headerPtr -> MessageType) {
                // switch for message type

                case ('S'):{
                    SystemEvent* event = reinterpret_cast<SystemEvent*>(ptr);; //creates struct
                    //assigns type since it got eaten by the header


                    //endianness swaps
                    event -> TrackingNumber = bswap16(event -> TrackingNumber);
                    uint64_t timestamp = parseTimestamp(event -> Timestamp);

                    //print for debug... no need to swap StockLocate since its 0
                    //std::cout << "The type is " << event -> MessageType << ", the locate is " << event -> StockLocate
                    //<< ", the tracking number is " << event -> TrackingNumber << ", the time was "
                    //<< timestamp << ", the event code is " << event -> EventCode << "." << std::endl;

                    break;
                }
                case ('R'): {
                    StockDirectory* msg;

                    msg = reinterpret_cast<StockDirectory*>(ptr);
                    //the whole length of each is sizeof(length)+sizeof(event), but the length variable only has sizeof(event)
                    //since we assigned the type manually (1 byte), we need to read length-1 bytes more

                    //reverse all integers, endianness
                    msg -> StockLocate = bswap16(msg -> StockLocate);
                    msg -> TrackingNumber = bswap16(msg -> TrackingNumber);
                    uint64_t timestamp = parseTimestamp(msg -> Timestamp);
                    msg -> RoundLotSize = bswap32(msg -> RoundLotSize);
                    msg -> ETPLeverageFactor = bswap32(msg -> ETPLeverageFactor);
                    //convert stock to a string
                    std::string symbol(msg -> Stock, 8); //slow but will keep for now

                    //std::cout << msg -> StockLocate << " stands for " << symbol << std::endl;
                    stock_directory[msg -> StockLocate] = symbol;

                    break;
                }

                case ('A'): {
                    AddOrder* msg;
                    msg = reinterpret_cast<AddOrder*>(ptr);

                    //the whole length of each is sizeof(length)+sizeof(event), but the length variable only has sizeof(event)
                    //since we assigned the type manually (1 byte), we need to read length-1 bytes more

                    //reverse all integers, endianness
                    const uint32_t shares  = bswap32(msg->Shares);
                    const uint64_t orderID = bswap64(msg->OrderReferenceNumber);
                    const uint32_t price   = bswap32(msg->Price);
                    const uint16_t locate  = bswap16(msg->StockLocate);
                    const char     side    = msg->BuySell;

                    //std::cout << "Sent " << msg -> BuySell << " order for " << msg -> Shares << " shares of "
                    //        << stock_directory[msg -> StockLocate] << " at the price of " << (msg -> Price/1000) << " -> " << msg -> Price%1000 << std::endl;

                    //add order to the book, disregard locate for now
                    book->addOrder(shares, orderID, locate, side, price);

                    break;
                }

                case ('F'): {
                    AddOrderWithMPID* msg;
                    msg = reinterpret_cast<AddOrderWithMPID*>(ptr);

                    const uint32_t shares  = bswap32(msg->Shares);
                    const uint64_t orderID = bswap64(msg->OrderReferenceNumber);
                    const uint32_t price   = bswap32(msg->Price);
                    const uint16_t locate  = bswap16(msg->StockLocate);
                    const char     side    = msg->BuySell;

                    //reverse all integers, endianness
                    //keep them like this for now even though they should have local vars
                    msg -> TrackingNumber = bswap16(msg -> TrackingNumber);
                    uint64_t timestamp = parseTimestamp(msg -> Timestamp);


                    //std::cout << "Sent " << msg -> BuySell << " order for " << msg -> Shares << " shares of "
                    //        << stock_directory[msg -> StockLocate] << " at the price of " << (msg -> Price/1000) << " -> " << msg -> Price%1000 << std::endl;

                    book->addOrder(shares, orderID, locate, side, price);


                    break;
                }

                case ('H'): {
                    StockTradingAction* msg;
                    msg = reinterpret_cast<StockTradingAction*>(ptr);

                    //reverse all integers, endianness
                    msg -> StockLocate = bswap16(msg -> StockLocate);
                    msg -> TrackingNumber = bswap16(msg -> TrackingNumber);
                    uint64_t timestamp = parseTimestamp(msg -> Timestamp);
                    break;
                }

                case ('E'): { //really cool how theres basically no executions compared to adds, todo measure this
                    OrderExecuted* msg;
                    msg = reinterpret_cast<OrderExecuted*>(ptr);

                    //reverse all integers, endianness
                    const uint64_t orderID = bswap64(msg->OrderReferenceNumber);
                    const uint16_t locate  = bswap16(msg->StockLocate);
                    const uint64_t shares = bswap64(msg -> ExecutedShares);

                    //keep for now, wrong! shouldnt edit directly
                    msg -> TrackingNumber = bswap16(msg -> TrackingNumber);
                    uint64_t timestamp = parseTimestamp(msg -> Timestamp);
                    msg -> MatchNumber = bswap64(msg -> MatchNumber); //unique

                    book->executeOrder(orderID, shares);

                    //std::cout << "Executed order for " << msg -> ExecutedShares << " of " << stock_directory[msg -> StockLocate] << std::endl;
                    break;
                }

                case ('C'): { //really cool how theres basically no executions compared to adds, todo measure this
                    OrderExecutedWithPrice* msg;
                    msg = reinterpret_cast<OrderExecutedWithPrice*>(ptr);

                    //reverse all integers, endianness
                    const uint64_t orderID = bswap64(msg->OrderReferenceNumber);
                    const uint16_t locate  = bswap16(msg->StockLocate);
                    const uint64_t shares = bswap64(msg -> ExecutedShares);

                    //keep for now, wrong! shouldnt edit directly
                    msg -> TrackingNumber = bswap16(msg -> TrackingNumber);
                    uint64_t timestamp = parseTimestamp(msg -> Timestamp);
                    msg -> MatchNumber = bswap64(msg -> MatchNumber); //unique
                    msg -> ExecutionPrice = bswap32(msg -> ExecutionPrice);

                    //std::cout << "Executed order for " << msg -> ExecutedShares << " of " << stock_directory[msg -> StockLocate]
                    //<< "at " << msg -> ExecutionPrice/1000 << " -> " << msg -> ExecutionPrice%1000 << std::endl;

                    book->executeOrder(orderID, shares);


                    break;
                }

                case ('D'): {
                    OrderDelete* msg;
                    msg = reinterpret_cast<OrderDelete*>(ptr);

                    //reverse all integers, endianness
                    uint64_t orderID = bswap64(msg -> OrderReferenceNumber);

                    //keep for now, bad to change directly
                    msg -> StockLocate = bswap16(msg -> StockLocate);
                    msg -> TrackingNumber = bswap16(msg -> TrackingNumber);
                    uint64_t timestamp = parseTimestamp(msg -> Timestamp);

                    //std::cout << "Canceled order for " << stock_directory[msg -> StockLocate] << std::endl;

                    book->cancelOrder(orderID);

                    break;
                }

                case ('U'): {
                    OrderReplace* msg;

                    msg = reinterpret_cast<OrderReplace*>(ptr);
                    //reverse all integers, endianness

                    uint64_t originalOrderID = bswap64(msg -> OriginalOrderReferenceNumber);
                    uint64_t newOrderID = bswap64(msg -> NewOrderReferenceNumber);
                    uint32_t shares = bswap32(msg -> Shares);
                    uint32_t price = bswap32(msg -> Price);
                    uint16_t locate = bswap16(msg -> StockLocate);
                    //keeping this for now
                    msg -> TrackingNumber = bswap16(msg -> TrackingNumber);
                    uint64_t timestamp = parseTimestamp(msg -> Timestamp);


                    book->replaceOrder(originalOrderID, newOrderID,
                        shares, price);
                    break;


                }


                default: {
                    //std::cout << "Unrecognized message type skipping..." << std::endl;
                    //the whole length of each is sizeof(length)+sizeof(event), but the length variable only has sizeof(event)
                    //since we assigned the type manually (1 byte), we need to read length-1 bytes more
                    break;
                }
            }
            //if (counter % 1000000 == 0) {
            //    book->printStats();
            //    std::cout << "-----------------------" << std::endl;
            //    std::cout << "Total Orders in Processed: " << counter << std::endl;
            //    std::cout << "-----------------------" << std::endl;
            //}
            ptr += length;
        }
        // --- STOP TIMER ---
        auto end_time = high_resolution_clock::now();

        // 2. Calculate Duration
        // We cast the difference to specific units (microseconds, nanoseconds, milliseconds)
        auto duration_us = duration_cast<microseconds>(end_time - start_time);
        auto duration_ns = duration_cast<nanoseconds>(end_time - start_time);

        // 3. The Adrenaline Report
        cout << "------------------------------------------------" << endl;
        cout << "Processed " << counter << " messages." << endl;
        cout << "Total Time: " << duration_us.count() << " microseconds ("
             << duration_us.count() / 10000.0 << " ms)" << endl;

        // The "HFT" Metric: Time per message
        double ns_per_msg = (long double)duration_ns.count() / counter;
        cout << "Latency: " << ns_per_msg << " ns/msg" << endl;
        cout << "Throughput: " << (counter / (duration_us.count() / 1000000.0)) << " msgs/sec" << endl;
        cout << "------------------------------------------------" << endl;

        // 1. Generate a runtime index based on the counter (compiler can't predict this)
        //size_t random_index = counter % (end - ptr);

        // 2. Read from the buffer
        //volatile char check = v[random_index];

        // 3. Print it (Forces the read to be real)
        //std::cout << "Sanity Check (Random Byte): " << (int)check << std::endl;
        //this is genuinely crazy what everything does the computer do and i dont have the slightest clue
        // it was at 5ns and i added this print and now its 13ns, just because the compiler is so smart it just skips
        //everything it doesnt need.

    }
}
#endif //THE_THING_PARSER_HPP