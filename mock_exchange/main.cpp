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
using namespace std;
int main() {

    int fd = open("/home/vozojk/THE THING/resources/market.bin", O_RDONLY); //get a file descriptor for the bin file

    struct stat st; //declare a metadata struct from /sys/ to hold the file metadata
    fstat(fd, &st); //load the metadata in

    char* mapped_data = (char*)mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    //using mmap to map directly into the processes virtual address space, casted to char* so we can od easy pointer arithmetic (char is a byte)


    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    //create a socket IPv4, UPD, default (0)
    //disable checksum since it could help rid of loopback
    int disable = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_NO_CHECK, &disable, sizeof(disable)) < 0) {
        perror("setsockopt SO_NO_CHECK failed");
    }

    sockaddr_in dest_addr{};
    dest_addr.sin_family = AF_INET;//IPv4 family
    dest_addr.sin_port = htons(12345); //big endian port
    dest_addr.sin_addr.s_addr = inet_addr("172.28.197.104"); //localhost ip address

    size_t offset = 0; //position in the file
    int counter = 1;

    auto start_time = std::chrono::high_resolution_clock::now(); //start clock

    while (offset < (size_t)st.st_size) { //loop for the entire file
        uint16_t msg_len = ntohs(*(uint16_t*)(mapped_data + offset)); //get the current 2 bytes (msg_len) from the header and convert to small endian
        offset += 2; //add the length of the uint16_t type to account for the actual msg_len variable not being included in the payload
        counter++;
        sendto(sock, mapped_data + offset, msg_len, 0, (struct sockaddr*)&dest_addr, sizeof(dest_addr));
        //send the payload with the msg_len separately

        offset += msg_len; //advance pointer to the next message

        //usleep(50);
        if (counter % 1000000 == 0) {

            break;
        }
    }
    auto end_time = std::chrono::high_resolution_clock::now();
    // --- STOP TIMER ---

//
        // 2. Calculate Duration
        // We cast the difference to specific units (microseconds, nanoseconds, milliseconds)
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

    munmap(mapped_data, st.st_size); //done with memory mapping
    //close things and return
    close(fd);
    close(sock);
    return 0;

}