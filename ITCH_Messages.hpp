//
// Created by vojte on 07/02/2026.
//

#ifndef THE_THING_ITCH_MESSAGES_HPP
#define THE_THING_ITCH_MESSAGES_HPP

#include <cstdint>

#pragma once
#pragma pack(push, 1)




struct MessageHeader {
    uint16_t Length; //message length
    char MessageType; //type for the message
};

struct SystemEvent {
    char MessageType; //type for the message ('S')
    uint16_t StockLocate; //id of stock, always 0 for system msg
    uint16_t TrackingNumber; //nasdaq tracking number
    uint8_t Timestamp[6]; //nanoseconds since midnight
    char EventCode; //code for event, see ITCH spec page 5
};

// check for system event size correctness
static_assert(sizeof(SystemEvent) == 12, "ITCH SystemEvent struct size mismatch!");

struct StockDirectory {
    char MessageType; //type for the message ('R');
    uint16_t StockLocate; // id of stock - connects to the tag
    uint16_t TrackingNumber; //nasdaq tracking number
    uint8_t Timestamp[6]; //nanoseconds since midnight
    char Stock[8]; //name for the stock i.e. 'AAPL    ', padded to 8 chars
    char MarketCategory; //indicates market or market tier (NYSE, Nasdaq Global Market)
    char FinancialStatusIndicator; //status, bankrupt, suspended etc...
    uint32_t RoundLotSize; //size of the lot
    char RoundLotsOnly; //restriction to trade only in lots (Y/N)
    char IssueClassification; //stock, etf, preffered
    char IssueSubtype[2]; //more divided types, etf, common etc...
    char Authenticity; //live/production, test/demo
    char ShortSaleThresholdIndicator; // Y/N, is yes when theres a lot of shorts, gets banned from more
    char IPOFlag; // Y/N, is IPO
    char LULDReferencePriceTier; //limits for LULD halt (big move fast)
    char ETPFlag; //Exchange traded product, like an ETF
    uint32_t ETPLeverageFactor; //how leveraged is the ETP
    char InverseIndicator; // is the ETP inverse of the underlying
};
//checkSize
static_assert(sizeof(StockDirectory) == 39, "StockDirectory size mismatch!");
//Stock Trading Action (1.2.2) State of each stock
struct StockTradingAction {
    char MessageType; //type for the message ('H');
    uint16_t StockLocate; // id of stock - connects to the tag
    uint16_t TrackingNumber; //nasdaq tracking number
    uint8_t Timestamp[6]; //nanoseconds since midnight
    char Stock[8]; //name for the stock i.e. 'AAPL    ', padded to 8 chars
    char TradingState; //H-Halted...
    char Reserved; //reserved - for later use
    char Reason[4]; // trading action reason
};
//Add Order (1.3.1): The source of liquidity.
//
struct AddOrder {
    char MessageType; //type for the message ('A');
    uint16_t StockLocate; // id of stock - connects to the tag
    uint16_t TrackingNumber; //nasdaq tracking number
    uint8_t Timestamp[6]; //nanoseconds since midnight
    uint64_t OrderReferenceNumber; //unique ref number
    char BuySell; //B-Buy, S-Sell
    uint32_t Shares; //num of shares
    char Stock[8]; //name for the stock i.e. 'AAPL    ', padded to 8 chars
    uint32_t Price; //price*10^4
};
struct AddOrderWithMPID {
    char MessageType; //type for the message ('F');
    uint16_t StockLocate; // id of stock - connects to the tag
    uint16_t TrackingNumber; //nasdaq tracking number
    uint8_t Timestamp[6]; //nanoseconds since midnight
    uint64_t OrderReferenceNumber; //unique ref number
    char BuySell; //B-Buy, S-Sell
    uint32_t Shares; //num of shares
    char Stock[8]; //name for the stock i.e. 'AAPL    ', padded to 8 chars
    uint32_t Price; //price*10^4
    char Attribution[4]; //participant identifier
};

//Order Executed (1.4.1): The signal that a trade happened.
//
struct OrderExecuted {
    char MessageType; //type for the message ('E');
    uint16_t StockLocate; // id of stock - connects to the tag
    uint16_t TrackingNumber; //nasdaq tracking number
    uint8_t Timestamp[6]; //nanoseconds since midnight
    uint64_t OrderReferenceNumber; //unique ref number
    uint32_t ExecutedShares; //number of shares affected
    uint64_t MatchNumber; //unique identifier
};
struct OrderExecutedWithPrice {
    char MessageType; //type for the message ('C');
    uint16_t StockLocate; // id of stock - connects to the tag
    uint16_t TrackingNumber; //nasdaq tracking number
    uint8_t Timestamp[6]; //nanoseconds since midnight
    uint64_t OrderReferenceNumber; //unique ref number
    uint32_t ExecutedShares; //number of shares affected
    uint64_t MatchNumber; //unique identifier
    char Printable; //should be reflected on calculations (if not then it is accounted for somewhere else)
    uint32_t ExecutionPrice; //price of execution *10^4
};
//Order Cancel/Delete (1.4.3/1.4.4): Book maintenance.
//
struct OrderCancel {
    char MessageType; //type for the message ('X');
    uint16_t StockLocate; // id of stock - connects to the tag
    uint16_t TrackingNumber; //nasdaq tracking number
    uint8_t Timestamp[6]; //nanoseconds since midnight
    uint64_t OrderReferenceNumber; //unique ref number
    uint32_t CanceledShares; //number of shares affected
};

struct OrderDelete {
    char MessageType; //type for the message ('D');
    uint16_t StockLocate; // id of stock - connects to the tag
    uint16_t TrackingNumber; //nasdaq tracking number
    uint8_t Timestamp[6]; //nanoseconds since midnight
    uint64_t OrderReferenceNumber; //unique ref number
};
//Order Replace (1.4.5): The price/size update.
struct OrderReplace { //basically editing the order
    char MessageType; //type for the message ('U');
    uint16_t StockLocate; // id of stock - connects to the tag
    uint16_t TrackingNumber; //nasdaq tracking number
    uint8_t Timestamp[6]; //nanoseconds since midnight
    uint64_t OriginalOrderReferenceNumber; //unique ref number of order
    uint64_t NewOrderReferenceNumber; //to be replaced with
    uint32_t Shares; //how many shares in the new order
    uint32_t Price; //new price*10^4
};

#pragma pack(pop)

#endif //THE_THING_ITCH_MESSAGES_HPP
