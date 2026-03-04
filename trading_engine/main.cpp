#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>

#include "Order_Book.hpp"
#include "Parser.hpp"

#include <vector>
#include <chrono>
using namespace std::chrono;
using namespace std;

int main() {
    // Pre-allocate a book for every possible StockLocate ID
    std::vector<OrderBook> AllBooks(65536);

    int sock = socket(AF_INET, SOCK_DGRAM, 0); //gets sock fd
    if (sock < 0) { //checks if the fd successfully got created
        perror("Socket creation failed");
        return 1;
    }
    //set fileds and make addr struct
    sockaddr_in local_addr{};
    local_addr.sin_family = AF_INET;
    local_addr.sin_port = htons(12345);//must match port in exhange/main.cpp
    local_addr.sin_addr.s_addr = INADDR_ANY;//dont care about ip

    //bind to the port so our program gets data from it
    if (bind(sock, (struct sockaddr*)&local_addr, sizeof(local_addr)) < 0) {
        perror("Bind failed");
        close(sock);
        return 1;//if the bind fails we return
    }

    std::cout << "Listening on port 12345" << std::endl;

    uint8_t buffer[2048]; //buffer for the packet, 2048 just to be sure
    //each should be max like 50 though
    int count = 1;

    auto batch_start_time = std::chrono::high_resolution_clock::now(); //start clock
    while (true) {//loop for new packet


        ssize_t received = recvfrom(sock, buffer, sizeof(buffer), 0, nullptr, nullptr);
        //makes it sleep until we get something then it loads it into the array

        if (received < 0) {//receive failed if recvfrom return -1
            perror("Receive failed");
            break;
        }
        count++;

        if (count % 100000 == 0) {
            auto batch_end_time = high_resolution_clock::now();

            // Calculate how long this batch of 100k took
            auto duration_us = duration_cast<microseconds>(batch_end_time - batch_start_time).count();

            // Throughput: (Messages / Microseconds) * 1,000,000 = Messages per Second
            double msgs_per_sec = (100000.0 / duration_us) * 1000000.0;

            // Latency: Average nanoseconds per message
            auto duration_ns = duration_cast<nanoseconds>(batch_end_time - batch_start_time).count();
            double ns_per_msg = (double)duration_ns / 100000.0;

            std::cout << "[TELEMETRY] Processed " << count << " msgs | "
                      << "Throughput: " << msgs_per_sec << " msgs/sec | "
                      << "Avg Latency: " << ns_per_msg << " ns/msg" << std::endl;

            // Reset the clock for the next batch
            batch_start_time = high_resolution_clock::now();
        }

        ITCHParser::parse(reinterpret_cast<char*>(buffer), (size_t) received, AllBooks.data());


    }

    close(sock);




}