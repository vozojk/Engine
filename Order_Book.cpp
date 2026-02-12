//
// Created by vojte on 09/02/2026.
//

#include "Order_Book.hpp"

#include <iostream>

#include "Types.hpp"

void OrderBook::addOrder(uint32_t Shares, uint64_t OrderID,
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
//cancels an order and updates the book at a certain price level accordingly, updates the rb tree(volume, delete from list) and hashmap (delete order)
void OrderBook::deleteOrder(uint64_t OrderID) {


        auto hashIt = Orders.find(OrderID);

        if (hashIt == Orders.end()) return;

        auto listIt = hashIt->second;

        PriceLevel *plList = listIt->Parent;
        uint32_t Price = plList->Price;
        char side = listIt->BuySell;
        uint64_t shares = listIt->Shares;
        //chat said i should create local variables since im potentially
        //erasing the PriceLevel. real reason is access from the object is stored in th
        //incredibly interesting, and also so simple, when i create a local variable
        //the cpu will move it into fast memory (cache,register) because it knows i will
        //use it a lot
        uint64_t &volume = plList->TotalVolume;

        volume -= shares;

        if (volume == 0) {
                if (side == 'S') {
                        Asks.erase(Price);
                }else {
                        Bids.erase(Price);
                }
        }else {
                plList->orders.erase(listIt);
        }

        Orders.erase(hashIt);

}
//this is basically the same as cancel order
//apparently the CPU keeps history of function calls and what happened after and makes predictions based on that
//by separating them i make it more deterministic, the cpu will be right more often
void OrderBook::reduceOrderSize(uint64_t OrderID, uint32_t executedShares) {
        auto hashIt = Orders.find(OrderID); //gives an iterator for the key value pair in the hashmap

        if (hashIt == Orders.end()) return;

        auto listIt = hashIt->second; //gives an iterator for the value in the hashmap at the iterator

        PriceLevel *plList = listIt->Parent;
        uint64_t &shares = listIt->Shares;
        uint64_t &volume = plList->TotalVolume;

        shares -= executedShares; //removes executedShares from the total shares of the order

        char side = listIt->BuySell;
        uint32_t Price = plList->Price;

        volume -= executedShares;

        //remove order if shares = 0, made a mistake, needs to be deleted first
        //by deleting the PriceLevel i delete the orders list too
        if (shares == 0) {

                plList->orders.erase(listIt); //remove Order from the list
                Orders.erase(hashIt); //remove iterator from the hashmap
        }
        //nesting this would be smart since i check for the volume only if shares are 0
        if (volume == 0) {
                if (side == 'S') {
                        Asks.erase(Price);
                }else {
                        Bids.erase(Price);
                }
        }
}

void OrderBook::replaceOrder(const uint64_t oldOrderID,
                        const uint64_t newOrderID,
                        const uint32_t Shares,
                        const uint32_t Price) {
        //we need to delete and add because replace resets time priority

        auto hashIt = Orders.find(oldOrderID); //gives an iterator for the key value pair in the hashmap

        // --- ADD THIS CHECK ---
        if (hashIt == Orders.end()) {
                return; // We missed the original order, so we can't replace it.
        }
        auto listIt = hashIt->second; //gives an iterator for the value in the hashmap at the iterator

        PriceLevel *plList = listIt->Parent;
        uint64_t &sharesInside = listIt->Shares;
        uint64_t &volume = plList->TotalVolume;

        //used later
        uint64_t locate = listIt->StockLocate;

        char side = listIt->BuySell;
        uint32_t oldPrice = plList->Price;

        volume -= sharesInside;

        plList->orders.erase(listIt); //remove Order from the list

        Orders.erase(hashIt);

        if (volume == 0 && oldPrice != Price) {
                if (side == 'S') {
                        Asks.erase(oldPrice);
                }else {
                        Bids.erase(oldPrice);
                }
        }

        PriceLevel &level = (side == 'B') ? Bids[Price] : Asks[Price];
        //to the map
        level.Price=Price;
        level.TotalVolume += Shares;
        level.orders.emplace_back(Shares ,newOrderID ,&level ,locate ,side);

        //to the hashmap
        auto it = std::prev(level.orders.end());
        Orders[newOrderID] = it;

}

void OrderBook::printStats() {
        std::cout << "--- MARKET SNAPSHOT ---" << std::endl;

        if (Bids.empty()) {
                std::cout << "Bids: EMPTY" << std::endl;
        } else {
                // Bids are sorted High-to-Low (std::greater), so begin() is the Highest Price
                auto bestBid = Bids.begin();
                std::cout << "Best Bid: " << bestBid->second.TotalVolume
                          << " shrs @ $" << (bestBid->first / 10000.0) << std::endl;
        }

        if (Asks.empty()) {
                std::cout << "Asks: EMPTY" << std::endl;
        } else {
                // Asks are sorted Low-to-High (default), so begin() is the Lowest Price
                auto bestAsk = Asks.begin();
                std::cout << "Best Ask: " << bestAsk->second.TotalVolume
                          << " shrs @ $" << (bestAsk->first / 10000.0) << std::endl;
        }

        std::cout << "Total Orders in Memory: " << Orders.size() << std::endl;
        std::cout << "-----------------------" << std::endl;
}

void OrderBook::cancelOrder(uint64_t OrderID, uint32_t Shares) {
        OrderBook::reduceOrderSize(OrderID, Shares);
}

void OrderBook::executeOrder(uint64_t OrderID, uint32_t Shares) {
        OrderBook::reduceOrderSize(OrderID, Shares);
}

//helper for private field
bool OrderBook::hasOrders() {
        return !Orders.empty();
}