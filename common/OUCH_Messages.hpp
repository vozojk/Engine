//
// Created by vojte on 14/02/2026.
//

#ifndef THE_THING_OUCH_MESSAGES_HPP
#define THE_THING_OUCH_MESSAGES_HPP

#pragma once
#pragma pack(push, 1)
#include <cstdint>


//S,A,C,E,B,J

enum class OrderState : uint8_t;

struct SystemEvent { //type = 'S'
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
    char OrderState; //L=live, D=dead
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
    char ClOrdID[14]; //order ID assigned by sender of the order (not NASDAQ)
    uint64_t Price;
    char Symbol[8];
    uint32_t UserRefNum; //starts at 1, global order counter
    uint32_t Quantity;
    char Type;
    char Side;
    char TimeInForce;
    char Display; //Y=visible, N=hidden, A=attributable, Z=conformant
    char Capacity;
    char InterMarketSweepEligibility; //Y/N
    char CrossType;

    //will not send any optionals
};

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














#pragma pack(pop)

#endif //THE_THING_OUCH_MESSAGES_HPP




