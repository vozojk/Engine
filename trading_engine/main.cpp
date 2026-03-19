#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cerrno>
#include <sys/epoll.h>
#include <fcntl.h>
#include <netinet/tcp.h>//to disable Nagle's algorithm

#include "Order_Book.hpp"
#include "Parser.hpp"
#include "OUCH_Parser.hpp"

#include <vector>
#include <chrono>

#include "Logger.hpp"

using namespace std::chrono;
using namespace std;

MainLogger engine_logger;

int set_nonblocking(int fd) { //sets a socket to nonblocking so an empty queue doesn't make it wait but it immediatelly returns and checks again
    int flags = fcntl(fd, F_GETFL, 0); //gets the sock flags
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK); //ORs the flags with the nonblock flag and sets them
}//returns EAGAIN if the queue is empty

int main() {

    //initialize the logger object
    engine_logger.start();

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
    set_nonblocking(udp_sock);
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

    engine_logger.log("Listening (UDP) on port 12345");


    //epoll UDP set
    ev.events = EPOLLIN | EPOLLET; //set flags for edge triggered wake-up, this way we only wake up if NEW data arrives
    ev.data.fd = udp_sock; //set udp sock
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, udp_sock, &ev); //configure for udp, add socket to RBT and make epoll listen for things in the socket (through ev)

    //TCP socket

    int tcp_sock = socket(AF_INET, SOCK_STREAM, 0); //init
    set_nonblocking(tcp_sock); //set nonblocking so ET works
    int flag = 1;
    setsockopt(tcp_sock, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
    //make address struct
    sockaddr_in tcp_addr{};
    tcp_addr.sin_family = AF_INET; //ipv4
    tcp_addr.sin_port = htons(9000);
    inet_pton(AF_INET, "127.0.0.1", &tcp_addr.sin_addr); //stores address
    int state = connect(tcp_sock, (struct sockaddr*)&tcp_addr, sizeof(tcp_addr)); //establish connection

    //logs
    if (state == 0) {
        engine_logger.log("Connection established on port 9000");
    } else if (state == -1) {
        if (errno == EINPROGRESS) {
            engine_logger.log("Handshake start successful");
        } else if (errno == ECONNREFUSED) {
            engine_logger.log("Connection refused");
        } else {
            // std::strerror returns a const char*, so we use %s
            engine_logger.log("Error: %s", std::strerror(errno));
        }
    } else {
        engine_logger.log("What happened?!?");
    }

    //epoll setup
    ev.events = EPOLLIN | EPOLLET | EPOLLOUT;
    ev.data.fd = tcp_sock;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, tcp_sock, &ev);


    uint8_t udpBuffer[2048]; //buffer for the packet, 2048 just to be sure
    uint8_t tcpBuffer[2048];
    std::vector<char> stage; //vector to keep unfinished packets
    //each should be max like 50 though
    int total = 0;
    int count = 1;
    auto batch_start_time = std::chrono::high_resolution_clock::now(); //start clock
    while (true) {//loop for new packet
        //sleeps until we get something
        int socketNum = epoll_wait(epoll_fd, events, 64, -1);

        for (int i = 0; i < socketNum; i++) {
            //UDP

            if (events[i].data.fd == udp_sock) {
                //loop until empty since we use edge triggered
                while (true) {
                    int size = recvfrom(udp_sock, udpBuffer, sizeof(udpBuffer), 0, nullptr, nullptr);

                    if (size < 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) break; // Buffer is empty! Go back to sleep.
                        perror("UDP recvfrom error");
                        break;
                    }
                    //todo could cause garbage data or segfaults, the length is 2 bytes but if the packet gets corrupted and sends one byte i will read unrelated data, need to check if size >=2
                    if (size > 0) {

                        size_t offset = 0;
                        int messages_in_bundle = 0;

                        while (offset < (size_t)size) {

                            uint16_t len;
                            std::memcpy(&len, udpBuffer+offset,2); //copy the 2 bytes with the lenght
                            len = ntohs(len); //to small-endian

                            offset+=2;//for len

                            if (offset + len > (size_t)size) {
                                std::cerr << "Corrupted packet: Incomplete message \n";
                                break;
                            }

                            char* msg = (char*)udpBuffer+offset;

                            count++;

                            ITCHParser::parse(msg, AllBooks, tcp_sock);

                            offset += len;
                            messages_in_bundle++;
                        }

                        //cout << "Divided packet into " << messages_in_bundle << " messages \n";
                    }else {//size == -1 when it disconnects
                        cout << "exchange disconnected \n"; break;
                    }
                }
                //TCP
            }else if (events[i].data.fd == tcp_sock) {
                //check if connection completed
                if (events[i].events & EPOLLOUT) { //epollout is set when sock is writable, this being set for the first time means conn established
                    int err = 0;
                    socklen_t errlen = sizeof(err);
                    getsockopt(tcp_sock, SOL_SOCKET, SO_ERROR, &err, &errlen);

                    if (err == 0) {
                        cout << "TCP connection established\n";

                        ev.events = EPOLLIN | EPOLLET; //epollout sends out all the time about the buffer being empty, it doesn't really get full we can optimize by not having it
                        ev.data.fd = tcp_sock;
                        epoll_ctl(epoll_fd, EPOLL_CTL_MOD, tcp_sock, &ev);
                    } else {
                        cerr << "TCP failed: " << strerror(err) << "\n";
                        close(tcp_sock);
                        return 0;
                    }
                }
                //ET
                while (true){
                    int size = recv(tcp_sock, tcpBuffer, sizeof(tcpBuffer), 0);
                    if (size == 0) {
                        cout << "peer disconnected \n";
                        break;
                    }
                    if (size < 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) break; // Buffer is empty! Go back to sleep.
                        perror("TCP recvfrom error");
                        break;
                    }

                    stage.insert(stage.end(), tcpBuffer, tcpBuffer+size);

                    while (!stage.empty()) {

                        size_t expected = sizeof(Accepted);

                        if (stage.size() < expected) {
                            cout << "INCOMPLETE \n";
                            break; //not enough bytes on the stage for a full message
                        }

                        count++;

                        OUCH::Inbound::parse(stage.data());

                        //todo this is atrociously slow, it moves the entire vector start at zero every time i call erase, we can use a circular buffer (like a ring buffer for the nic) implemented with a normal array
                        stage.erase(stage.begin(), stage.begin()+expected);



                    }
                }
            }

        }

        if (count > 1000000) {



            auto batch_end_time = high_resolution_clock::now();

            // Calculate how long this batch of 1M took
            auto duration_us = duration_cast<microseconds>(batch_end_time - batch_start_time).count();

            // Throughput: (Messages / Microseconds) * 1,000,000 = Messages per Second
            //todo why the hell did moving the 1M inside make it so much faster??
            double msgs_per_sec = ((count* 1000000.0) / duration_us) ;

            // Latency: Average nanoseconds per message
            auto duration_ns = duration_cast<nanoseconds>(batch_end_time - batch_start_time).count();
            double ns_per_msg = (double)duration_ns / count;

            std::cout << "[TELEMETRY] Processed " << count << " msgs | "
                      << "Throughput: " << msgs_per_sec << " msgs/sec | "
                      << "Avg Latency: " << ns_per_msg << " ns/msg | Total: " << total << std::endl;

            total+=count; //reset counter and add it to total
            count = 1;

            // Reset the clock for the next batch
            batch_start_time = high_resolution_clock::now();
        }




    }
    //not reachable right now but i would forget later
    close(tcp_sock);
    close(epoll_fd);
    close(udp_sock);
}