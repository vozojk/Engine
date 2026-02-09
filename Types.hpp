//
// Created by vojte on 09/02/2026.
//
#ifndef THE_THING_TYPES_HPP
#define THE_THING_TYPES_HPP

#include <cstdint>
#include <list>

struct PriceLevel;

struct Order { //ordered to not waste space since theres padding
    uint64_t Shares;
    uint64_t OrderID;
    PriceLevel* Parent;
    uint16_t StockLocate;
    char BuySell;
};

struct PriceLevel {
    uint64_t TotalVolume;
    std::list<Order*> orders;
    uint32_t Price;

};



#endif //THE_THING_TYPES_HPP