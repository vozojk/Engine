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
    inline OrderState* tracker = new OrderState[1000000];

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
        inline void parse(char* ptr, char* end, OrderBook books[]) { //need to handle partial messages

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

                        break;
                    }

                    case 'C': {
                        Cancelled* msg = reinterpret_cast<Cancelled*>(ptr);
                        ptr+=sizeof(Cancelled);

                        break;
                    }

                    case 'E': {
                        Executed* msg = reinterpret_cast<Executed*>(ptr);
                        ptr+=sizeof(Executed);

                        break;
                    }

                    case 'B': {
                        Broken* msg = reinterpret_cast<Broken*>(ptr);
                        ptr+=sizeof(Broken);

                        break;
                    }

                    case 'J': {
                        Rejected* msg = reinterpret_cast<Rejected*>(ptr);
                        ptr+=sizeof(Rejected);

                        break;
                    }

                    default: {
                        cout << "Message not recognized! BAD!!!" << std::endl;
                        cout << "Cannot move ptr! BAD!!!";
                    }

                }

            }
        }
    }

    namespace Outbound {
        inline uint32_t userRefNum = 1; //should be persistent, needs to recover if app restarted


        inline EnterOrder enterOrder(char Side, char Symbol[8], uint64_t Price, uint32_t Quantity) {
            EnterOrder order;

            order.Type = 'O';
            order.UserRefNum = bswap32(userRefNum); //could use htobe64 but this is more direct (htobe checks for cpu endianness)
            userRefnum++;
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
            OrderState* state = &tracker[userRefNum-1];
            state->Price = Price;
            state->TotalQuantity = Quantity;
            state->filledQuantity = 0;
            memcpy(state->Symbol, Symbol, 8);
            state->active = true;

        }
    }


}
#endif //THE_THING_OUCH_PARSER_HPP