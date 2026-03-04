//
// Created by vojte on 04/03/2026.
//
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

int main() {

    int fd = open("/home/vozojk/THE THING/resources/market.bin", O_RDONLY); //get a file descriptor for the bin file

    struct stat st; //declare a metadata struct from /sys/ to hold the file metadata
    fstat(fd, &st); //load the metadata in

    char* mapped_data = (char*)mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    //using mmap to map directly into the processes virtual address space, casted to char* so we can od easy pointer arithmetic (char is a byte)


    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    //create a socket IPv4, UPD, default (0)


    sockaddr_in dest_addr{};
    dest_addr.sin_family = AF_INET;//IPv4 family
    dest_addr.sin_port = htons(12345); //big endian port
    dest_addr.sin_addr.s_addr = inet_addr("127.0.0.1"); //localhost ip address

    size_t offset = 0; //position in the file

    while (offset < (size_t)st.st_size) { //loop for the entire file
        uint16_t msg_len = ntohs(*(uint16_t*)(mapped_data + offset)); //get the current 2 bytes (msg_len) from the header and convert to small endian
        offset += 2; //add the length of the uint16_t type to account for the actual msg_len variable not being included in the payload

        sendto(sock, mapped_data + offset, msg_len, 0, (struct sockaddr*)&dest_addr, sizeof(dest_addr));
        //send the payload with the msg_len separately

        offset += msg_len; //advance pointer to the next message

        usleep(50);
    }

    munmap(mapped_data, st.st_size); //done with memory mapping
    //close things and return
    close(fd);
    close(sock);
    return 0;

}