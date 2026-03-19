//
// Created by vojte on 09/02/2026.
//
#ifndef THE_THING_TYPES_HPP
#define THE_THING_TYPES_HPP

#include <cstdint>
#include <list>

#include "Logger.hpp"

struct PriceLevel;

struct Order { //ordered to not waste space since theres padding
    uint64_t Shares;
    uint64_t OrderID;
    PriceLevel* Parent;
    uint16_t StockLocate;
    char BuySell;

    Order(uint64_t pShares, uint64_t pOrderID, PriceLevel* pParent,
            uint16_t pStockLocate, char pBuySell) : Shares(pShares), OrderID(pOrderID), Parent(pParent), StockLocate(pStockLocate), BuySell(pBuySell) {
    }
};

struct PriceLevel {
    uint64_t TotalVolume;
    std::list<Order> orders;
    uint32_t Price;

    PriceLevel() {
        TotalVolume = 0;
        Price = 0;
    }
};

// Define the type once so all files agree on the capacity
using MainLogger = Logger<65536>;

// EXTERN tells the compiler: "This variable exists somewhere in the
// binary, but don't create it here. Just reserve the name."
extern MainLogger engine_logger;



#endif //THE_THING_TYPES_HPP