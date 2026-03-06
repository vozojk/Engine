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

#include "Order_Book.hpp"
#include "../common/ITCH_Messages.hpp"

#include <cstdint>
#include <vector>
#include <chrono> // <--- THE HEADER

#include "OUCH_Parser.hpp"
using namespace std::chrono;
using namespace std;


namespace ITCHParser {
    // A Lookup Table: Index = Locate ID, Value = Stock Symbol
    // 65536 is enough for all US stocks (max is usually ~12,000)
    inline std::vector<std::string> stock_directory(65536);

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

    static inline void showStock(int i, std::vector<OrderBook> &books) {
        if (books[i].hasOrders()) {
            std::cout << "\033[H\033[2J" << std::flush;
            std::cout << "StockLocate " << i << " " << stock_directory[i] << " is active:" << std::endl;
            books[i].printStats();

        }
    }

    // TIP To <b>Run</b> code, press <shortcut actionId="Run"/> or click the <icon src="AllIcons.Actions.Execute"/> icon in the gutter.
    inline void parse(char* ptr, std::vector<OrderBook> &books, int sock) {
        //now the function runs for 1 packet only
            switch (ptr[0]) {
                // switch for message type

                case ('S'):{
                    SystemEventITCH* event = reinterpret_cast<SystemEventITCH*>(ptr);; //creates struct
                    //assigns type since it got eaten by the header


                    //endianness swaps
                    event -> TrackingNumber = bswap16(event -> TrackingNumber);
                    uint64_t timestamp = parseTimestamp(event -> Timestamp);
                    char code = event -> EventCode;

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

                    //add order to the book, disregard locate for now
                    books[locate].addOrder(shares, orderID, locate, side, price);
                    const char symbol[8] = "APPL   ";
                    if (stock_directory[locate].starts_with("AAPL")) {
                        EnterOrder respond = OUCH::Outbound::enterOrder('B', "APPL    ", (price+1), shares);
                        send(sock, &respond, sizeof(respond), 0);
                        showStock(locate, books);
                    }

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


                    books[locate].addOrder(shares, orderID, locate, side, price);


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

                case ('E'): { //really cool how theres basically no executions compared to adds
                    OrderExecuted* msg;
                    msg = reinterpret_cast<OrderExecuted*>(ptr);

                    //reverse all integers, endianness
                    const uint64_t orderID = bswap64(msg->OrderReferenceNumber);
                    const uint16_t locate  = bswap16(msg->StockLocate);
                    const uint32_t shares = bswap32(msg -> ExecutedShares);

                    //keep for now, wrong! shouldnt edit directly
                    msg -> TrackingNumber = bswap16(msg -> TrackingNumber);
                    uint64_t timestamp = parseTimestamp(msg -> Timestamp);
                    msg -> MatchNumber = bswap64(msg -> MatchNumber); //unique

                    books[locate].executeOrder(orderID, shares);

                    break;
                }

                case ('C'): { //really cool how theres basically no executions compared to adds
                    OrderExecutedWithPrice* msg;
                    msg = reinterpret_cast<OrderExecutedWithPrice*>(ptr);

                    //reverse all integers, endianness
                    const uint64_t orderID = bswap64(msg->OrderReferenceNumber);
                    const uint16_t locate  = bswap16(msg->StockLocate);
                    const uint32_t shares = bswap32(msg -> ExecutedShares);

                    //keep for now, wrong! shouldnt edit directly
                    msg -> TrackingNumber = bswap16(msg -> TrackingNumber);
                    uint64_t timestamp = parseTimestamp(msg -> Timestamp);
                    msg -> MatchNumber = bswap64(msg -> MatchNumber); //unique
                    msg -> ExecutionPrice = bswap32(msg -> ExecutionPrice);


                    books[locate].executeOrder(orderID, shares);


                    break;
                }

                case ('D'): {
                    OrderDelete* msg;
                    msg = reinterpret_cast<OrderDelete*>(ptr);

                    //reverse all integers, endianness
                    uint64_t orderID = bswap64(msg -> OrderReferenceNumber);
                    uint16_t locate = bswap16(msg -> StockLocate);
                    //keep for now, bad to change directly
                    msg -> TrackingNumber = bswap16(msg -> TrackingNumber);
                    uint64_t timestamp = parseTimestamp(msg -> Timestamp);

                    books[locate].deleteOrder(orderID);

                    break;
                }

                case ('X'): {
                    OrderCancel* msg;
                    msg = reinterpret_cast<OrderCancel*>(ptr);
                    //reverse all integers, endianness
                    uint64_t orderID = bswap64(msg -> OrderReferenceNumber);
                    uint16_t locate = bswap16(msg -> StockLocate);
                    //keep for now, bad to change directly
                    msg -> TrackingNumber = bswap16(msg -> TrackingNumber);
                    uint64_t timestamp = parseTimestamp(msg -> Timestamp);
                    uint32_t shares = bswap32(msg -> CanceledShares);

                    books[locate].cancelOrder(orderID, shares);

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


                    books[locate].replaceOrder(originalOrderID, newOrderID,
                        shares, price);
                    break;


                }


                default: {
                    //std::cout << "Unrecognized message type skipping..." << std::endl;
                    break;
                }
            }


        }
}
#endif //THE_THING_PARSER_HPP