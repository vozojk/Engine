//
// Created by vojte on 14/02/2026.
//

#ifndef THE_THING_OUCH_MESSAGES_HPP
#define THE_THING_OUCH_MESSAGES_HPP

#pragma once
#pragma pack(push, 1)
#include <cstdint>


//S,A,C,E,B,J

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

struct Executed { //type = 'E'
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

struct OrderState {//price, total, filled, symbol, active

    uint64_t Price;
    uint32_t TotalQuantity;
    uint32_t filledQuantity;
    char Symbol[8];
    bool active;
};

//need to be able to fill orders, mark
//need to update parse to actually do something
//cases E,J,C,B













#pragma pack(pop)

#endif //THE_THING_OUCH_MESSAGES_HPP




