cmake_minimum_required(VERSION 3.20)
project(THE_THING)  # <--- Name updated

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# The Go Fast Flags
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -march=native -DNDEBUG")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0 -Wall -Wextra")
endif()

# We will add files here as we create them
add_executable(THE_THING main.cpp
        ITCH_Messages.hpp)