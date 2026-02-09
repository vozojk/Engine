//
// Created by vojte on 09/02/2026.
//

#ifndef THE_THING_ORDER_BOOK_HPP
#define THE_THING_ORDER_BOOK_HPP
#include <cstdint>
#include <map>
#include <unordered_map>

#include "Types.hpp"


class OrderBook {
    std::map<uint32_t, PriceLevel*, std::greater<uint32_t>> Bids;
    std::map<uint32_t, PriceLevel*> Asks;
    std::unordered_map<uint64_t, std::list<Order*>::iterator> Orders;

public:
    void addOrder(uint64_t Shares,
                    uint64_t OrderID,
                    uint16_t StockLocate,
                    char BuySell,
                    uint32_t Price);

    void cancelOrder(uint64_t OrderID);

    void executeOrder(uint64_t OrderID, uint64_t Shares);

    void executeTrade(uint32_t Price, uint64_t Shares);

    ~OrderBook();

};


#endif //THE_THING_ORDER_BOOK_HPP