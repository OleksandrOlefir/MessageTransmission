# Minimum CMake version required
cmake_minimum_required(VERSION 3.10)
# Project name and language
project(MessageSystem C)

# Set C standard to C11
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Define executables and their source files
add_executable(main src/main.c)
target_include_directories(main PRIVATE src)

add_executable(udp_sender src/udp_sender.c)
target_include_directories(udp_sender PRIVATE src)

add_executable(tcp_receiver src/tcp_receiver.c)
target_include_directories(tcp_receiver PRIVATE src)

# Link with POSIX threads
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(main Threads::Threads)
target_link_libraries(udp_sender Threads::Threads)
target_link_libraries(tcp_receiver Threads::Threads)