//
// Created by vojte on 09/02/2026.
//

#include "Order_Book.hpp"
#include "Types.hpp"

std::map<uint32_t, PriceLevel, std::greater<uint32_t>> Bids;
std::map<uint32_t, PriceLevel> Asks;
std::unordered_map<uint64_t, std::list<Order>::iterator> Orders;

void addOrder(uint64_t Shares, uint64_t OrderID,
        uint16_t StockLocate, char BuySell, uint32_t Price) {

        PriceLevel &level = (BuySell == 'B') ? Bids[Price] : Asks[Price];
        //to the map
        level.Price=Price;
        level.TotalVolume += Shares;
        level.orders.emplace_back(Shares,OrderID,&level,StockLocate,BuySell);

        //to the hashmap
        auto it = std::prev(level.orders.end());
        Orders[OrderID] = it;

}