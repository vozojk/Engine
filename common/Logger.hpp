//
// Created by vojte on 19/03/2026.
//

#ifndef HFT_SYSTEM_LOGGER_HPP
#define HFT_SYSTEM_LOGGER_HPP
#pragma once
#include <atomic>
#include <thread>
#include <cstdarg>
#include <cstdio>
#include <x86intrin.h>

using namespace std;

struct LogEntry {
    uint64_t timestamp;
    char message[256];
};

template <size_t Capacity>

class Logger {
    // check for powers of two, this makes division when doing the mod wrapping into a simple bitshift
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be a power of 2");
private:
    //create array (ring buffer) of structs with messages
    LogEntry storage[Capacity];

    //set as atomic, makes the threads not cook eachother (conflicts)
    alignas(64) atomic<size_t> head{0};
    alignas(64) atomic<size_t> tail{0};
    //for graceful termination
    atomic<bool> running{false};
    thread printingGuy;

    void consumer() {
        char buffer[65536];

        while (running.load(memory_order::relaxed)) {
            //we can do relaxed since the only thread who can modify the tail is consumer, saved time, no flushes required
            size_t curTail = tail.load(memory_order::relaxed);

            //head can get modified by the main thread so we need to update our values to be sure it's up to date
            size_t curHead = head.load(memory_order::acquire);

            if (curTail != curHead) {
                size_t offset = 0;
                while (curTail != curHead && offset < 65000){
                    //get everything from the ring buffer
                    LogEntry& entry = storage[curTail];

                    //load things into the main buffer from the ring buffer
                    int bytes_written = snprintf(buffer + offset, 65536 - offset, "[%lu] %s\n", entry.timestamp, entry.message);

                    if (bytes_written > 0) offset += bytes_written;

                    curTail = (curTail + 1) & (Capacity - 1); //the trick to do bitshifting instead of division
                }

                //store back tail, log can edit it now
                tail.store(curTail, memory_order_release);

                //write to stdout, flush it to print
                if (offset > 0) {
                    fwrite(buffer, 1, offset, stdout);
                    fflush(stdout);
                }
            }else {
                _mm_pause(); //we do this to save cpu power and to allow other things to access the core to do work
            }
        }
    }

public:
    Logger() = default;

    void start() {
        running = true;
        printingGuy = thread(&Logger::consumer, this);
    }

    void stop() {
        running = false;
        if (printingGuy.joinable()) {
            printingGuy.join();
        }
    }

    void log(const char* format, ...) {
        size_t curHead = head.load(memory_order::relaxed);

        //capacity-1 is a mask with all 1s except MSB which makes the result h+1 every time except for
        //when it gets to 10000... where the 1 gets deleted and the number loops back to 0
        //this is why capacity must be a pow of 2
        size_t nextHead = (curHead+1) & (Capacity-1);

        //check if the ring buffer is full (tail is right in front of the head)

        //we use mem order acquire since it disallows the cpu (and compiler) from reordering instructions which could then mess with how the cpu writes and reads
        //when writing the data first gets stored into a store buffer and then get written into the cache in some order
        //this would normally be fine since the cpu will invalidate the data but if the thred is busy and tries to access it before refetching it will break
        //which could otherwise break the logic since it doesn't consider other threads in reordering
        //forces the cpu to flush the invalidation queue and get everything sorted before running this
        if (nextHead == tail.load(memory_order::acquire)) {
          return;
        }

        //format string into the struct
        LogEntry &entry = storage[curHead]; //get address for the struct
        entry.timestamp = __rdtsc(); //get cpu time

        va_list args;
        va_start(args, format);
        vsnprintf(entry.message, sizeof(entry.message), format, args);
        va_end(args);

        //allow instruction reordering again since we did the thing where it mattered
        //more directly, it forces the cpu to flush the queue and all the data to be written before continuing
        //note: this whole cpu queue thing is only true for some CPUs, x86 uses total order which doesn't scramble instructions
        //but the compiler still does so here it is a compiler directive
        head.store(nextHead, memory_order::release);

    }

};



#endif //HFT_SYSTEM_LOGGER_HPP