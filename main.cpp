#include <iostream>
#include <fstream>
#include "ITCH_Messages.hpp"

// Put this at the top of your main.cpp or protocol.h

#include <cstdint>

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

    while (file) {

        MessageHeader header;
        file.read(reinterpret_cast<char*>(&header), sizeof(header)); //read the header part and create the object

        if (file.gcount() == 0) break; //check if there was something

        switch (header.MessageType) {
            // switch for message type

            case ('S'):{
                SystemEvent event; //creates struct
                event.MessageType = header.MessageType; //assigns type since it got eaten by the header
                file.read(reinterpret_cast<char*>(&event.StockLocate), bswap16(header.Length) - 1);
                //the whole length of each is sizeof(length)+sizeof(event), but the length variable only has sizeof(event)
                //since we assigned the type manually (1 byte), we need to read length-1 bytes more


                //endianness swaps
                event.TrackingNumber = bswap16(event.TrackingNumber);
                uint64_t timestamp = parseTimestamp(event.Timestamp);

                //print for debug...
                std::cout << "The type is " << event.MessageType << ", the locate is " << event.StockLocate
                << ", the tracking number is " << event.TrackingNumber << ", the time was "
                << timestamp << ", the event code is " << event.EventCode << "." << std::endl;

                break;
            }

            default: {
                //std::cout << "Unrecognized message type skipping..." << std::endl;

                file.seekg(bswap16(header.Length) - 1, std::ios::cur);
                //the whole length of each is sizeof(length)+sizeof(event), but the length variable only has sizeof(event)
                //since we assigned the type manually (1 byte), we need to read length-1 bytes more
                break;
            }
        }
    }

}