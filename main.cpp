#include <iostream>
#include <fstream>
#include "ITCH_Messages.hpp"

// Put this at the top of your main.cpp or protocol.h

#include <cstdint>
#include <vector>
#include <chrono> // <--- THE HEADER
using namespace std::chrono;
using namespace std;

// A Lookup Table: Index = Locate ID, Value = Stock Symbol
// 65536 is enough for all US stocks (max is usually ~12,000)
std::vector<std::string> stock_directory(65536);
int counter = 0;

// 1. Swap 16-bit (2 bytes) - used for Lengths and Message Types
uint16_t bswap16(uint16_t x) {
    return __builtin_bswap16(x);
}

// 2. Swap 32-bit (4 bytes) - used for Prices and Shares
uint32_t bswap32(uint32_t x) {
    return __builtin_bswap32(x);
}

// 3. Swap 64-bit (8 bytes) - used for Order IDs and Timestamps
uint64_t bswap64(uint64_t x) {
    return __builtin_bswap64(x);
}

//get timestamp into a 64 bit number
uint64_t parseTimestamp(uint8_t* x) {
    return ((uint64_t)x[0] << 40 |
            (uint64_t)x[1] << 32 |
            (uint64_t)x[2] << 24 |
            (uint64_t)x[3] << 16 |
            (uint64_t)x[4] << 8 |
            (uint64_t)x[5]);
}

// TIP To <b>Run</b> code, press <shortcut actionId="Run"/> or click the <icon src="AllIcons.Actions.Execute"/> icon in the gutter.
int main() {

    //move the binary file to RAM (drive -> kernel cache(in RAM))
    std::ifstream file("/home/vozojk/THE THING/market.bin", std::ios::binary);

    //check if it actually loaded
    if (!file) {
        std::cerr << "FILE NOT READ SUCCESSFULLY" << std::endl;
        return -1;
    }

    std::cout << "ENGINE STARTING!" << std::endl;
    auto start_time = high_resolution_clock::now(); //start clock
    while (counter < 1000000) { //switch to while (file) for full file

        MessageHeader header;
        file.read(reinterpret_cast<char*>(&header), sizeof(header)); //read the header part and create the object

        if (file.gcount() == 0) break; //check if there was something
        header.Length = bswap16(header.Length);
        //count for latency measurements
        counter++;
        switch (header.MessageType) {
            // switch for message type

            case ('S'):{
                SystemEvent event; //creates struct
                event.MessageType = header.MessageType; //assigns type since it got eaten by the header
                file.read(reinterpret_cast<char*>(&event.StockLocate), header.Length - 1);
                //the whole length of each is sizeof(length)+sizeof(event), but the length variable only has sizeof(event)
                //since we assigned the type manually (1 byte), we need to read length-1 bytes more


                //endianness swaps
                event.TrackingNumber = bswap16(event.TrackingNumber);
                uint64_t timestamp = parseTimestamp(event.Timestamp);

                //print for debug... no need to swap StockLocate since its 0
                //std::cout << "The type is " << event.MessageType << ", the locate is " << event.StockLocate
                //<< ", the tracking number is " << event.TrackingNumber << ", the time was "
                //<< timestamp << ", the event code is " << event.EventCode << "." << std::endl;

                break;
            }
            case ('R'): {
                StockDirectory msg;
                msg.MessageType = header.MessageType; //assigns type since it got eaten by the header
                file.read(reinterpret_cast<char*>(&msg.StockLocate), header.Length - 1);
                //the whole length of each is sizeof(length)+sizeof(event), but the length variable only has sizeof(event)
                //since we assigned the type manually (1 byte), we need to read length-1 bytes more

                //reverse all integers, endianness
                msg.StockLocate = bswap16(msg.StockLocate);
                msg.TrackingNumber = bswap16(msg.TrackingNumber);
                uint64_t timestamp = parseTimestamp(msg.Timestamp);
                msg.RoundLotSize = bswap32(msg.RoundLotSize);
                msg.ETPLeverageFactor = bswap32(msg.ETPLeverageFactor);
                //convert stock to a string
                std::string symbol(msg.Stock, 8); //slow but will keep for now

                //std::cout << msg.StockLocate << " stands for " << symbol << std::endl;
                stock_directory[msg.StockLocate] = symbol;

                break;
            }

            case ('A'): {
                AddOrder msg;
                msg.MessageType = header.MessageType; //assigns type since it got eaten by the header
                file.read(reinterpret_cast<char*>(&msg.StockLocate), header.Length - 1);
                //the whole length of each is sizeof(length)+sizeof(event), but the length variable only has sizeof(event)
                //since we assigned the type manually (1 byte), we need to read length-1 bytes more

                //reverse all integers, endianness
                msg.StockLocate = bswap16(msg.StockLocate);
                msg.TrackingNumber = bswap16(msg.TrackingNumber);
                uint64_t timestamp = parseTimestamp(msg.Timestamp);
                msg.OrderReferenceNumber = bswap64(msg.OrderReferenceNumber);
                msg.Shares = bswap32(msg.Shares);
                msg.Price = bswap32(msg.Price);

                //std::cout << "Sent " << msg.BuySell << " order for " << msg.Shares << " shares of "
                //        << stock_directory[msg.StockLocate] << " at the price of " << (msg.Price/1000) << "." << msg.Price%1000 << std::endl;

                break;
            }

            case ('F'): {
                AddOrderWithMPID msg;
                msg.MessageType = header.MessageType; //assigns type since it got eaten by the header
                file.read(reinterpret_cast<char*>(&msg.StockLocate), header.Length - 1);
                //the whole length of each is sizeof(length)+sizeof(event), but the length variable only has sizeof(event)
                //since we assigned the type manually (1 byte), we need to read length-1 bytes more

                //reverse all integers, endianness
                msg.StockLocate = bswap16(msg.StockLocate);
                msg.TrackingNumber = bswap16(msg.TrackingNumber);
                uint64_t timestamp = parseTimestamp(msg.Timestamp);
                msg.OrderReferenceNumber = bswap64(msg.OrderReferenceNumber);
                msg.Shares = bswap32(msg.Shares);
                msg.Price = bswap32(msg.Price);

                //std::cout << "Sent " << msg.BuySell << " order for " << msg.Shares << " shares of "
                //        << stock_directory[msg.StockLocate] << " at the price of " << (msg.Price/1000) << "." << msg.Price%1000 << std::endl;

                break;
            }

            case ('H'): {
                StockTradingAction msg;

                msg.MessageType = header.MessageType; //assigns type since it got eaten by the header
                file.read(reinterpret_cast<char*>(&msg.StockLocate), header.Length - 1);
                //the whole length of each is sizeof(length)+sizeof(event), but the length variable only has sizeof(event)
                //since we assigned the type manually (1 byte), we need to read length-1 bytes more

                //reverse all integers, endianness
                msg.StockLocate = bswap16(msg.StockLocate);
                msg.TrackingNumber = bswap16(msg.TrackingNumber);
                uint64_t timestamp = parseTimestamp(msg.Timestamp);
                break;
            }

            case ('E'): { //really cool how theres basically no executions compared to adds, todo measure this
                OrderExecuted msg;
                msg.MessageType = header.MessageType; //assigns type since it got eaten by the header
                file.read(reinterpret_cast<char*>(&msg.StockLocate), header.Length - 1);
                //the whole length of each is sizeof(length)+sizeof(event), but the length variable only has sizeof(event)
                //since we assigned the type manually (1 byte), we need to read length-1 bytes more

                //reverse all integers, endianness
                msg.StockLocate = bswap16(msg.StockLocate);
                msg.TrackingNumber = bswap16(msg.TrackingNumber);
                uint64_t timestamp = parseTimestamp(msg.Timestamp);
                msg.OrderReferenceNumber = bswap64(msg.OrderReferenceNumber); //not really necessary, unique anyway
                msg.ExecutedShares = bswap32(msg.ExecutedShares);
                msg.MatchNumber = bswap64(msg.MatchNumber); //unique

                //std::cout << "Executed order for " << msg.ExecutedShares << " of " << stock_directory[msg.StockLocate] << std::endl;
                break;
            }

            case ('C'): { //really cool how theres basically no executions compared to adds, todo measure this
                OrderExecutedWithPrice msg;
                msg.MessageType = header.MessageType; //assigns type since it got eaten by the header
                file.read(reinterpret_cast<char*>(&msg.StockLocate), header.Length - 1);
                //the whole length of each is sizeof(length)+sizeof(event), but the length variable only has sizeof(event)
                //since we assigned the type manually (1 byte), we need to read length-1 bytes more

                //reverse all integers, endianness
                msg.StockLocate = bswap16(msg.StockLocate);
                msg.TrackingNumber = bswap16(msg.TrackingNumber);
                uint64_t timestamp = parseTimestamp(msg.Timestamp);
                msg.OrderReferenceNumber = bswap64(msg.OrderReferenceNumber); //not really necessary, unique anyway
                msg.ExecutedShares = bswap32(msg.ExecutedShares);
                msg.MatchNumber = bswap64(msg.MatchNumber); //unique
                msg.ExecutionPrice = bswap32(msg.ExecutionPrice);

                //std::cout << "Executed order for " << msg.ExecutedShares << " of " << stock_directory[msg.StockLocate]
                //<< "at " << msg.ExecutionPrice/1000 << "." << msg.ExecutionPrice%1000 << std::endl;
                break;
            }

            case ('D'): {
                OrderCancel msg;
                msg.MessageType = header.MessageType; //assigns type since it got eaten by the header
                file.read(reinterpret_cast<char*>(&msg.StockLocate), header.Length - 1);
                //the whole length of each is sizeof(length)+sizeof(event), but the length variable only has sizeof(event)
                //since we assigned the type manually (1 byte), we need to read length-1 bytes more

                //reverse all integers, endianness
                msg.StockLocate = bswap16(msg.StockLocate);
                msg.TrackingNumber = bswap16(msg.TrackingNumber);
                uint64_t timestamp = parseTimestamp(msg.Timestamp);
                msg.OrderReferenceNumber = bswap64(msg.OrderReferenceNumber); //not really necessary, unique anyway

                //std::cout << "Canceled order for " << stock_directory[msg.StockLocate] << std::endl;

                break;
            }

            case ('U'): {
                OrderReplace msg;
                msg.MessageType = header.MessageType; //assigns type since it got eaten by the header
                file.read(reinterpret_cast<char*>(&msg.StockLocate), header.Length - 1);
                //the whole length of each is sizeof(length)+sizeof(event), but the length variable only has sizeof(event)
                //since we assigned the type manually (1 byte), we need to read length-1 bytes more

                //reverse all integers, endianness
                msg.StockLocate = bswap16(msg.StockLocate);
                msg.TrackingNumber = bswap16(msg.TrackingNumber);
                uint64_t timestamp = parseTimestamp(msg.Timestamp);
                msg.OriginalOrderReferenceNumber = bswap64(msg.OriginalOrderReferenceNumber); //not really necessary, unique anyway
                msg.Shares = bswap32(msg.Shares);
                msg.Price = bswap32(msg.Price);
                break;
            }


            default: {
                //std::cout << "Unrecognized message type skipping..." << std::endl;

                file.seekg(header.Length - 1, std::ios::cur);
                //the whole length of each is sizeof(length)+sizeof(event), but the length variable only has sizeof(event)
                //since we assigned the type manually (1 byte), we need to read length-1 bytes more
                break;
            }
        }
    }
    // --- STOP TIMER ---
    auto end_time = high_resolution_clock::now();

    // 2. Calculate Duration
    // We cast the difference to specific units (microseconds, nanoseconds, milliseconds)
    auto duration_us = duration_cast<microseconds>(end_time - start_time);
    auto duration_ns = duration_cast<nanoseconds>(end_time - start_time);

    // 3. The Adrenaline Report
    cout << "------------------------------------------------" << endl;
    cout << "Processed " << counter << " messages." << endl;
    cout << "Total Time: " << duration_us.count() << " microseconds ("
         << duration_us.count() / 1000.0 << " ms)" << endl;

    // The "HFT" Metric: Time per message
    double ns_per_msg = (double)duration_ns.count() / counter;
    cout << "Latency: " << ns_per_msg << " ns/msg" << endl;
    cout << "Throughput: " << (counter / (duration_us.count() / 1000000.0)) << " msgs/sec" << endl;
    cout << "------------------------------------------------" << endl;

}