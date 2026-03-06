//
// Created by vojte on 04/03/2026.
//
#include <chrono>
#include <sys/socket.h>   // Core socket API
#include <netinet/in.h>   // Internet address family
#include <arpa/inet.h>    // IP/Byte-ordering utils
#include <sys/mman.h>     // Memory mapping (mmap)
#include <sys/stat.h>     // File status (fstat)
#include <fcntl.h>        // File control (open)
#include <unistd.h>       // POSIX API (close/sleep)
#include <cstring>        // memory manipulation
#include <cstdint>        // standard integer types
#include <iostream>       // console logging
#include <thread>         // multithreading
#include <netinet/tcp.h>

#include "OUCH_Messages.hpp"
using namespace std;
void logTime(auto start_time, auto end_time, int counter) {

    auto duration_us = chrono::duration_cast<chrono::microseconds>(end_time - start_time);
    auto duration_ns = chrono::duration_cast<chrono::nanoseconds>(end_time - start_time);

    // 3. The Adrenaline Report
    cout << "------------------------------------------------" << endl;
    cout << "Processed " << counter << " messages." << endl;
    cout << "Total Time: " << duration_us.count() << " microseconds ("
         << duration_us.count() / 1000.0 << " ms)" << endl;

    // The "HFT" Metric: Time per message
    double ns_per_msg = (long double)duration_ns.count() / counter;
    cout << "Latency: " << ns_per_msg << " ns/msg" << endl;
    cout << "Throughput: " << (counter / (duration_us.count() / 1000000.0)) << " msgs/sec" << endl;
    cout << "------------------------------------------------" << endl;
}

void sendBundledData(const char* filename, int udp_sock, const struct sockaddr_in dest) {
    sleep(10);
    int fd = open(filename, O_RDONLY); //get a file descriptor for the bin file
    if (fd < 0) {
        perror("File failed to open!");
        return;
    }

    struct stat st; //declare a metadata struct from /sys/ to hold the file metadata
    fstat(fd, &st); //load the metadata in

    char* mapped_data = (char*)mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    //using mmap to map directly into the processes virtual address space, casted to char* so we can od easy pointer arithmetic (char is a byte)

    if (mapped_data == MAP_FAILED) {
        perror("mmap failed");
        close(fd);
        return;
    }

    const size_t MTU_LIMIT = 1400;
    size_t bundle_start = 0; //start of the bundle
    auto start_time = std::chrono::high_resolution_clock::now(); //start clock

    size_t current_offset = 0; //position in the file
    int counter = 1;

    while (current_offset < (size_t)st.st_size) { //loop for the entire file
        //this can apparently cause segfaults with mmap, todo reseach and fix
        uint16_t msg_len = ntohs(*(uint16_t*)(mapped_data + current_offset)); //get the current 2 bytes (msg_len) from the header and convert to small endian

        counter++;

        uint16_t len = msg_len+2; //include the len part of the header
        if (current_offset+len > st.st_size) break;

        size_t bundle_size = current_offset - bundle_start;

        //send (bundle full)
        if (bundle_size + len > MTU_LIMIT && bundle_size > 0) {
            sendto(udp_sock, mapped_data + bundle_start, bundle_size, 0, (sockaddr*) &dest, sizeof(dest));
            //cout << "sent " << bundle_size << "bytes. About " << bundle_size/30 << "packets \n";
            bundle_start = current_offset;
        }
        current_offset += len; //advance pointer to the next message
        //cout << "sent" << len << "bytes";
        usleep(5);
        if (counter % 1000000 == 0) {
            auto end_time = std::chrono::high_resolution_clock::now();
            logTime(start_time, end_time, 1000000);
            start_time = std::chrono::high_resolution_clock::now(); //start clock
        }
        if (counter % 2000000000 == 0) {//stop
            break;
        }
    }

    if (current_offset > bundle_start) {//final flush
        sendto(udp_sock, mapped_data + bundle_start, current_offset - bundle_start, 0, (sockaddr*) &dest, sizeof(dest));
    }

    munmap(mapped_data, st.st_size); //done with memory mapping
    //close things and return
    close(fd);
}

void tcpResponder(int fd) {

    cout << "Waiting for connection \n";
    //wait for the accept socket of the engine
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    int sock = accept(fd, (sockaddr*) &addr, &len);

    if (sock < 0) {
        perror("Accept failed");
        return;
    }

    cout << "Engine connected \n";

    char buffer[2048];
    uint64_t exchange_order_id = 1;

    while (true) {
        ssize_t size = recv(sock, buffer, sizeof(buffer), 0);
        if (size > 0) {
            int processed = 0;
            //loop to respond to all the messages in case they got packaged up
            while (processed + sizeof(EnterOrder) <= size) {
                //check for enter order
                if (buffer[processed] == 'O') {

                    EnterOrder* incoming = reinterpret_cast<EnterOrder *>(buffer+processed);

                    Accepted back;
                    back.Type = 'A';

                    auto now = std::chrono::high_resolution_clock::now().time_since_epoch();
                    back.Timestamp = htobe64(std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());

                    memcpy(back.ClOrdID, incoming->ClOrdID, 14);
                    memcpy(back.Symbol, incoming->Symbol, 8);
                    back.UserRefNum = incoming->UserRefNum;
                    back.Price = incoming->Price;
                    back.Quantity = incoming->Quantity;
                    back.Side = incoming->Side;
                    back.TimeInForce = incoming->TimeInForce;
                    back.Display = incoming->Display;
                    back.Capacity = incoming->Capacity;
                    back.InterMarketSweepEligibility = incoming->InterMarketSweepEligibility;
                    back.CrossType = incoming->CrossType;

                    back.State = 'L';
                    back.OrderID = htobe64(exchange_order_id);
                    exchange_order_id++;

                    send(sock, &back, sizeof(back), 0);

                    cout << "Sent ACK for UserRefNum: " << ntohl(back.UserRefNum) << " Exchange orderID: " << exchange_order_id-1 << "\n";

                    processed += sizeof(EnterOrder);
                }
            }
        } else if (size == 0) {
            cout << "engine disconnected\n";
            break;
        }

    }


    close(sock);

}
//inialize sockets and start threads for tcp and udp
int main() {


    //udp
    int udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
    //create a socket IPv4, UPD, default (0)
    //disable checksum since it could help rid of loopback
    int disable = 1;
    if (setsockopt(udp_sock, SOL_SOCKET, SO_NO_CHECK, &disable, sizeof(disable)) < 0) {
        perror("setsockopt SO_NO_CHECK failed");
    }

    sockaddr_in udp_addr{};
    udp_addr.sin_family = AF_INET;//IPv4 family
    udp_addr.sin_port = htons(12345); //big endian port
    udp_addr.sin_addr.s_addr = inet_addr("127.0.0.1"); //localhost ip address

    std::thread udp(sendBundledData, "/home/vozojk/THE THING/resources/market.bin", udp_sock, udp_addr);
    cout << "UDP thread started\n";

    //tcp
    int tcp_sock = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(tcp_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    setsockopt(tcp_sock, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));

    sockaddr_in tcp_addr{};
    tcp_addr.sin_family = AF_INET;
    tcp_addr.sin_addr.s_addr = INADDR_ANY;
    tcp_addr.sin_port = htons(9000);

    bind(tcp_sock, (sockaddr*)&tcp_addr, sizeof(tcp_addr));
    listen(tcp_sock, 3);

    std::thread tcp(tcpResponder, tcp_sock);

    tcp.join();
    udp.join();

    close(tcp_sock);
    close(udp_sock);
    return 0;

}