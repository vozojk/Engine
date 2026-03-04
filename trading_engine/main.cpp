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

    while (true) {//loop for new packet


        ssize_t received = recvfrom(sock, buffer, sizeof(buffer), 0, nullptr, nullptr);
        //makes it sleep until we get something then it loads it into the array

        if (received < 0) {//receive failed if recvfrom return -1
            perror("Receive failed");
            break;
        }

        char msg_type = (char)buffer[0];
        //std::cout << "Recieved packet! Type: " << msg_type << " (" << received << " bytes)" << std::endl;
        ITCHParser::parse(reinterpret_cast<char*>(buffer), (size_t) received, AllBooks.data());


    }

    close(sock);




}