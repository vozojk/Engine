//
// Created by vojte on 14/02/2026.
//

#ifndef THE_THING_OUCH_PARSER_HPP
#define THE_THING_OUCH_PARSER_HPP

#include <iostream>
#include <fstream>

#include "Order_Book.hpp"

#include <cstdint>
#include <vector>
#include <chrono> // <--- THE HEADER
#include <cstring>

#include "OUCH_Messages.hpp"
using namespace std::chrono;
using namespace std;

namespace OUCH {
    inline MyOrder* tracker = new MyOrder[1000000]; //preallocated, my orders are small, instant access and mutation by id
    inline unordered_map<uint64_t, ExecutionRecord> fillTracker; //key is the match number
    inline uint32_t userRefNum = 1; //should be persistent, needs to recover if app restarted

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

    namespace Inbound {



        // TIP To <b>Run</b> code, press <shortcut actionId="Run"/> or click the <icon src="AllIcons.Actions.Execute"/> icon in the gutter.
        inline void parse(char* ptr, char* end) { //need to handle partial messages

            while (ptr < end) {
                char Type = *reinterpret_cast<char*>(ptr);

                switch (Type) {
                    case 'S': {
                        SystemEvent* msg = reinterpret_cast<SystemEvent*>(ptr);
                        ptr+=sizeof(SystemEvent);
                        break;
                    }

                    case 'A': {
                        Accepted* msg = reinterpret_cast<Accepted*>(ptr);
                        ptr+=sizeof(Accepted);
                        MyOrder* order = &tracker[bswap32(msg->UserRefNum)-1];

                        order->active = OrderState::LIVE;
                        break;
                    }

                    case 'C': {
                        Cancelled* msg = reinterpret_cast<Cancelled*>(ptr);
                        ptr+=sizeof(Cancelled);

                        MyOrder* order = &tracker[bswap32(msg->UserRefNum)-1];

                        uint32_t open = order->openQuantity;

                        open -= bswap32(msg->Quantity);

                        if (open == 0) order->active = OrderState::CANCELED;
                        order->openQuantity = open;

                        break;
                    }

                    case 'E': {
                        Executed* msg = reinterpret_cast<Executed*>(ptr);
                        ptr+=sizeof(Executed);

                        MyOrder* order = &tracker[bswap32(msg->UserRefNum)-1];

                        uint32_t open = order->openQuantity;
                        uint32_t filled = bswap32(msg->Quantity);
                        open -= filled;
                        order->openQuantity = open;
                        order->filledQuantity += filled;

                        //update fill tracker
                        fillTracker[bswap64(msg->MatchNumber)].filled_qty += filled;

                        if (open == 0) order->active = OrderState::FILLED;
                        else order->active = OrderState::PARTIALLY_FILLED;

                        break;
                    }

                    case 'B': {
                        Broken* msg = reinterpret_cast<Broken*>(ptr);
                        ptr+=sizeof(Broken);

                        uint32_t filled = fillTracker[bswap64(msg->MatchNumber)].filled_qty;

                        MyOrder* order = &tracker[bswap32(msg->UserRefNum)-1];

                        order->filledQuantity -= filled;
                        order->totalQuantity -= filled;
                        //need to change the amount of shares owned and get money back
                        //it does not void the whole trade even though there is no quantity
                        //i need to figure out how much from my own book
                        //i have created the fillTracker for this, it stores order
                        //with match numbers separately so the normal book doesnt
                        //get bloated

                        break;
                    }

                    case 'J': {
                        Rejected* msg = reinterpret_cast<Rejected*>(ptr);
                        ptr+=sizeof(Rejected);

                        MyOrder* order = &tracker[bswap32(msg->UserRefNum)-1];

                        order->active = OrderState::REJECTED;
                        //should change monies later if i have them
                        break;
                    }

                    default: {
                        cout << "Message not recognized! Aborting buffer parse." << std::endl;
                        return; // Escape the while loop entirely
                    }

                }

            }
        }
    }

    namespace Outbound {


        inline EnterOrder enterOrder(char Side, char Symbol[8], uint64_t Price, uint32_t Quantity) {
            EnterOrder order;

            order.Type = 'O';
            order.UserRefNum = bswap32(userRefNum); //could use htobe64 but this is more direct (htobe checks for cpu endianness)

            order.Side = Side;
            order.Quantity = bswap32(Quantity);
            memset(order.Symbol, ' ', 8);
            memcpy(order.Symbol, Symbol, 8);
            order.Price = bswap64(Price);
            order.TimeInForce = 'D';
            order.Display = 'Y';
            order.Capacity = 'P';
            order.InterMarketSweepEligibility = 'N';
            order.CrossType = 'N';
            memset(order.ClOrdID, '0', 14);
            memcpy(order.ClOrdID, "VOJTE", 5);

            // 3. Convert UserRefNum to ASCII digits, writing from right-to-left
            uint32_t tempNum = userRefNum-1;
            int idx = 13; // Start at the end of the 14-byte array

            // Special case for 0 (otherwise loop skips it)
            if (tempNum == 0) {
                order.ClOrdID[idx] = '0';
            } else {
                // Peel digits until we run out of number OR run out of space (9 digits max)
                while (tempNum > 0 && idx >= 5) {
                    order.ClOrdID[idx--] = (tempNum % 10) + '0';
                    tempNum /= 10;
                }
            }
            //creates the struct for keeping track of created order and adds it to the book
            MyOrder* myOrder = &tracker[userRefNum-1];
            myOrder->price = Price;
            myOrder->totalQuantity = Quantity;
            myOrder->openQuantity = Quantity;
            myOrder->filledQuantity = 0;
            memcpy(myOrder->symbol, Symbol, 8);
            myOrder->active = OrderState::PENDING_NEW;

            userRefNum++;

            return order;



        }
    }


}
#endif //THE_THING_OUCH_PARSER_HPP