//
// Created by vojte on 14/02/2026.
//


#pragma once
#pragma pack(push, 1)
#include <cstdint>


//S,A,C,E,B,J

enum class OrderState : uint8_t;

struct SystemEventOUCH { //type = 'S'
    char Type;
    uint64_t Timestamp;
    char EventCode;
};

struct Accepted { // type = 'A'
    char Type;
    uint64_t Timestamp;
    uint32_t UserRefNum;
    char Side;
    uint32_t Quantity;
    char Symbol[8];
    uint64_t Price;
    char TimeInForce;
    char Display; //Y=visible, N=hidden, A=attributable, Z=conformant
    uint64_t OrderID;
    char Capacity;
    char InterMarketSweepEligibility; //Y/N
    char CrossType;
    char State; //L=live, D=dead
    char ClOrdID[14]; //order ID assigned by sender of the order (not NASDAQ)
    //will not send any optionals
};

struct Cancelled { //full or partial cancel of the order - type = 'C'
    char Type;
    uint64_t Timestamp;
    uint32_t UserRefNum;
    uint32_t Quantity;
    char Reason;
};

struct Executed { //type = 'E' Marks a partial or total filling of order
    char Type;
    uint64_t Timestamp;
    uint32_t UserRefNum;
    uint32_t Quantity;
    uint64_t Price;
    char LiquidityFlag;
    uint64_t MatchNumber;
    //no optionals
};

struct Broken { // type = 'B'
    char Type;
    uint64_t Timestamp;
    uint32_t UserRefNum;
    uint64_t MatchNumber;
    char Reason; //E-erroneous, C-consent, both parties agreed to break, S-supervisory, X-broken by third party
    char ClOrdID[14]; //Internal order id assigned by the sender
    //no optionals
};

struct Rejected { // type = 'J'
    char Type;
    uint64_t Timestamp;
    uint32_t UserRefNum;
    char Reason; //E-erroneous, C-consent, both parties agreed to break, S-supervisory, X-broken by third party
    char ClOrdID[14]; //Internal order id assigned by the sender
    //no optionals
};

struct EnterOrder { // type = 'O'
    char Type;
    uint32_t UserRefNum; //starts at 1, global order counter
    char Side;
    uint32_t Quantity;
    char Symbol[8];
    uint64_t Price;
    char TimeInForce;
    char Display; //Y=visible, N=hidden, A=attributable, Z=conformant
    char Capacity;
    char InterMarketSweepEligibility; //Y/N
    char CrossType;
    char ClOrdID[14]; //order ID assigned by sender of the order (not NASDAQ)

    //will not send any optionals
};

#pragma pack(pop)
//here because of CPU alignment, the padding is generally good and useful, the CPU uses it so the memory is aligned to chunks it can process efficiently, we strip it away
//only for the network part because then the unpadded raw stream of fields wouldn't fit the struct (since there would be padding)
struct MyOrder {//price, total, filled, symbol, active

    uint64_t price;
    uint32_t totalQuantity;
    uint32_t filledQuantity;
    uint32_t openQuantity;
    char symbol[8];
    OrderState active;
};

enum class OrderState : uint8_t {
    UNALLOCATED = 0,
    PENDING_NEW = 1,
    LIVE = 2,
    PARTIALLY_FILLED = 3,
    FILLED = 4,
    CANCELED = 5,
    REJECTED = 6
};

struct ExecutionRecord {
    uint64_t MatchNumber;
    uint32_t filled_qty;
    uint32_t price;
};

//need to be able to fill orders, mark
//need to update parse to actually do something
//cases E,J,C,B
//
//Preallocate order space for the whole day. Keep a daily id of orders.
//goal is to manage an order book of my orders through enter order (so far) and the ouch responses i will simulate later.





