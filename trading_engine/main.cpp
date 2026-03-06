#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cerrno>
#include <sys/epoll.h>
#include <fcntl.h>

#include "Order_Book.hpp"
#include "Parser.hpp"
#include "OUCH_Parser.hpp"

#include <vector>
#include <chrono>

using namespace std::chrono;
using namespace std;

int set_nonblocking(int fd) { //sets a socket to nonblocking so an empty queue doesn't make it wait but it immediatelly returns and checks again
    int flags = fcntl(fd, F_GETFL, 0); //gets the sock flags
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK); //ORs the flags with the nonblock flag and sets them
}//returns EAGAIN if the queue is empty

int main() {
    // Pre-allocate a book for every possible StockLocate ID
    std::vector<OrderBook> AllBooks(65536);

    //epoll init
    int epoll_fd = epoll_create1(0); //allocates the RBT for epoll
    struct epoll_event ev{}, events[64]; //ev to configure what to scan and events for the ready list

    // UDP socket
    int udp_sock = socket(AF_INET, SOCK_DGRAM, 0); //gets sock fd
    if (udp_sock < 0) { //checks if the fd successfully got created
        perror("Socket creation failed");
        return 1;
    }
    //set fileds and make addr struct
    sockaddr_in local_addr{};
    local_addr.sin_family = AF_INET;
    local_addr.sin_port = htons(12345);//must match port in exhange/main.cpp
    local_addr.sin_addr.s_addr = INADDR_ANY;//dont care about ip

    //bind to the port so our program gets data from it
    if (bind(udp_sock, (struct sockaddr*)&local_addr, sizeof(local_addr)) < 0) {
        perror("Bind failed");
        close(udp_sock);
        return 1;//if the bind fails we return
    }

    std::cout << "Listening (UDP) on port 12345" << std::endl;


    //epoll UDP set
    ev.events = EPOLLIN | EPOLLET; //set flags for edge triggered wake-up, this way we only wake up if NEW data arrives
    ev.data.fd = udp_sock; //set udp sock
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, udp_sock, &ev); //configure for udp, add socket to RBT and make epoll listen for things in the socket (through ev)

    //TCP socket

    int tcp_sock = socket(AF_INET, SOCK_STREAM, 0); //init
    set_nonblocking(tcp_sock); //set nonblocking so ET works

    //make address struct
    sockaddr_in tcp_addr{};
    tcp_addr.sin_family = AF_INET; //ipv4
    tcp_addr.sin_port = htons(9000);
    inet_pton(AF_INET, "127.0.0.1", &tcp_addr.sin_addr); //stores address
    int state = connect(tcp_sock, (struct sockaddr*)&tcp_addr, sizeof(tcp_addr)); //establish connection

    //logs
    if (state == 0) {
        cout << "Connection established on port 9000 \n";
    }else if (state == -1) {
        if (errno == EINPROGRESS) {
            cout << "Handshake start successfull \n";
        }else if (errno == ECONNREFUSED) {
            cout << "Connection refused \n";
        }else {
            cout << "Error: " << std::strerror(errno) << "\n";
        }
    }else cout << "What happened?!?";

    //epoll setup
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = tcp_sock;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, tcp_sock, &ev);

    uint8_t buffer[2048]; //buffer for the packet, 2048 just to be sure
    //each should be max like 50 though
    int count = 1;
    int lastUDp = 0;
    auto batch_start_time = std::chrono::high_resolution_clock::now(); //start clock
    while (true) {//loop for new packet
        //sleeps until we get something
        int socketNum = epoll_wait(epoll_fd, events, 64, -1);

        for (int i = 0; i < socketNum; i++) {
            //UDP
            if (events[i].data.fd == udp_sock) {
                //loop until empty since we use edge triggered
                while (recvfrom(udp_sock, buffer, sizeof(buffer), 0, nullptr, nullptr)) {
                    ITCHParser::parse(reinterpret_cast<char*>(buffer), AllBooks.data());
                }
                //TCP
            }else if (events[i].data.fd == tcp_sock) {
                //ET
                while (recv(tcp_sock, buffer, sizeof(buffer), 0) > 0) {
                    OUCH::Inbound::parse(reinterpret_cast<char*>(buffer));
                }
            }

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




    }

    close(udp_sock);




}