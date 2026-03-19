//
// Created by vojte on 04/03/2026.
//
//todo use perf to track where do i lose performance and valgrind --tool=cachegrind to track cache misses
#include <chrono>
#include <sys/socket.h>   // Core socket API
#include <netinet/in.h>   // Internet address family
#include <arpa/inet.h>    // IP/Byte-ordering utils
#include <sys/mman.h>     // Memory mapping (mmap)
#include <sys/stat.h>     // File status (fstat)
#include <fcntl.h>        // File control (open)
#include <unistd.h>       // POSIX API (close/sleep)
#include <cstring>        // memory manipulation
#include <iostream>       // console logging
#include <thread>         // multithreading
#include <netinet/tcp.h>
#include <x86intrin.h>

#include "Logger.hpp"
#include "OUCH_Messages.hpp"
#include "Types.hpp"
using namespace std;

MainLogger exchange_logger;

void busy_loop(uint64_t nanoseconds, double ghz) {
    uint64_t cycles_to_wait = static_cast<uint64_t>(nanoseconds * ghz);
    uint64_t start = __rdtsc();

    // Spin the CPU until the cycle count is reached
    //todo cool assembly link under the hood, it is possible to call the pause thing directly (__asm__ volatile ("pause")), take a look in inline assembly
    while (__rdtsc() - start < cycles_to_wait) {
        _mm_pause();
        // This hint tells the CPU it's in a spin-loop, saving power
        // and improving performance for the next instruction.
    }
}
void logTime(auto start_time, auto end_time, int counter) {

    auto duration_us = chrono::duration_cast<chrono::microseconds>(end_time - start_time);
    auto duration_ns = chrono::duration_cast<chrono::nanoseconds>(end_time - start_time);

    // 3. The Adrenaline Report
    exchange_logger.log("------------------------------------------------");

    exchange_logger.log("Processed %lu messages.", (unsigned long)counter);

    exchange_logger.log("Total Time: %lld microseconds (%.3f ms)",
                        (long long)duration_us.count(),
                        (double)(duration_us.count() / 1000.0));

    // The "HFT" Metric: Time per message
    double ns_per_msg = (long double)duration_ns.count() / counter;
    exchange_logger.log("Latency: %.2f ns/msg", (double)ns_per_msg);

    exchange_logger.log("Throughput: %.2f msgs/sec",
                        (double)(counter / (duration_us.count() / 1000000.0)));

    exchange_logger.log("------------------------------------------------");
}

void sendBundledData(const char* filename, int udp_sock, const struct sockaddr_in dest, int sleep_ns) {
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
        //apparently it is linked to accessing data we dont own, for example if i am right at the end of the file, the line will try to access 16 bits, but there might be only 8 (1 byte) left
        //since we operate with raw non-virtual memory, this would just throw a segfault since there might be something completely different in the last 8 bits.
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
        //need to use an if wrap since otherwise usleep gets called even with 0, it is a sched_yield() syscall which damages performance a lot for this usecase
        //it is also incredibly interesting that when im at sleep(0), the engine gets overwhelmed, obviously since the throughput is like 400k/sec,
        //but the point is epoll never even gets to the TCP packets because the UDP queue is completely overflowing effectivelly stuck in a
        //events[i].data.fd == udp_sock condition while(true) loop since that breaks only with an error or EAGAIN
        //todo read quota to limit the numbers of packets read so this doesnt happen
        //if (sleep_us > 0) {
        //    usleep(sleep_us);
        //}
        //even more crazy that this runs much much faster at 5000ns than usleep at 1us
        busy_loop(sleep_ns, 1);
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

    exchange_logger.log("Waiting for connection \n");
    //wait for the accept socket of the engine
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    int sock = accept(fd, (sockaddr*) &addr, &len);

    if (sock < 0) {
        perror("Accept failed");
        return;
    }

    exchange_logger.log("Engine connected \n");

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

                    exchange_logger.log("Sent ACK for UserRefNum: %d Exchange orderID: %d\n", ntohl(back.UserRefNum), exchange_order_id-1);

                    processed += sizeof(EnterOrder);
                }
            }
        } else if (size == 0) {
            exchange_logger.log("engine disconnected\n");
            break;
        }

    }


    close(sock);

}
//inialize sockets and start threads for tcp and udp
int main(int argc, char* argv[]) {

    //initialize the logging object
    exchange_logger.start();

    int sleep_ns = 5000;
    //set sleep duration
    if (argc > 1) {
        try {
            sleep_ns = std::stoi(argv[1]);

            if (sleep_ns < 0) sleep_ns = 0;
        } catch (const std::exception& e) {
            std:cerr << "INVALID ARGUMENT. Setting Default (5us)";
            sleep_ns = 5000;
        }
    }
    exchange_logger.log("Starting exchange with UDP cannon delay of %d nanoseconds.\n", sleep_ns);

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

    std::thread udp(sendBundledData, "/home/vozojk/THE THING/resources/market.bin", udp_sock, udp_addr, sleep_ns);
    exchange_logger.log("UDP thread started\n");

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

    exchange_logger.stop();


    return 0;

}