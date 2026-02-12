#include <iostream>
#include <fstream>

#include "Order_Book.hpp"
#include "Types.hpp"
#include "Parser.hpp"
#include "ITCH_Messages.hpp"
// Put this at the top of your main.cpp or protocol.h

#include <cstdint>
#include <vector>
#include <chrono> // <--- THE HEADER
using namespace std::chrono;
using namespace std;

int main() {
    // Pre-allocate a book for every possible StockLocate ID
    std::vector<OrderBook> AllBooks(65536);
    std::vector<char> v(4000000000); //20 mil bytes
    //move the binary file to RAM (drive -> kernel cache(in RAM))
    std::ifstream file("/home/vozojk/THE THING/market.bin", std::ios::binary);

    //check if it actually loaded
    if (!file) {
        std::cerr << "FILE NOT READ SUCCESSFULLY" << std::endl;
        return -1;
    }

    OrderBook GlobalBook;

    file.read(v.data(), v.size()); //read file into one big array, lowers syscalls which are slow

    ITCHParser::parse(&v[0], &v[0]+v.size(), AllBooks.data());

}