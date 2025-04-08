# Message System Project

This project implements a message transmission system with three threads: two for receiving UDP messages and one for transmitting specific messages via TCP. The system meets all the requirements outlined, including non-blocking sockets, thread-safe message storage, duplicate filtering, and performance optimizations. The project is written in C/C++, uses a CMake build system, and targets Linux as the main platform.

## How to Build and Run the Project

### Prerequisites
- Linux operating system
- CMake (version 3.10 or higher)
- GCC (supporting C11)
- POSIX threads library (usually included with GCC)

### Build Instructions
1. Create a build directory:
   ```bash
   mkdir build
   cd build

### Generate build files with CMake:
2. cmake ..

### Compile the project:
3. make

### Run Instructions
1. Open three terminal windows.
2. In Terminal 1, start the TCP receiver:
   ```bash
    ./tcp_receiver
3. In Terminal 2, start the main application:
   ```bash
    ./main
4. In Terminal 3, start the UDP sender:
   ```bash
    ./udp_sender

## Requirements and Implementation Details
1. Two Threads Receiving Messages via UDP

    Implementation:
        Two threads (receiverThread) are created in main.c, each listening on a different UDP port (5000 and 5001).
        Each thread uses a UDP socket created with socket(AF_INET, SOCK_DGRAM, 0) and bound to its respective port using bind.
        The threads run concurrently, receiving messages in a loop until the program terminates (after 10 seconds).
    Technique:
        POSIX threads (pthread_t) are used for concurrency, managed via the thread_utils.h abstractions (thread_create, thread_join).
        Each thread operates independently, listening on its own socket, which avoids contention on the socket level.
    Why It Works:
        Using separate threads for each UDP port allows parallel processing of incoming messages, ensuring that one receiver doesn’t block the other.
        The threads are lightweight and managed efficiently with POSIX threads, suitable for a Linux environment.

2. Message Format

    Implementation:
        The message format is defined in message.h as a struct
            typedef struct {
            uint16_t MessageSize; // Size of the message
            uint64_t MessageId;   // Unique identifier for the message
            uint64_t MessageData; // Data payload
        } Message;
        The MessageSize field is set to sizeof(Message), and MessageId and MessageData are used for identification and filtering.
        Messages are converted to network byte order (htons, htonll) before sending and back to host byte order (ntohs, ntohll) after receiving to ensure portability across different architectures.
        htonll (and similarly ntohll) are not standard POSIX functions. While htonl and ntohs are standard for 32-bit and 16-bit conversions, respectively, there is no standard htonll or ntohll for 64-bit values in POSIX. Some systems provide these functions as extensions (e.g., glibc on Linux), but they are not guaranteed to be available. In the project, htonll and ntohll are used to convert 64-bit fields (MessageId and MessageData in the Message struct) between host and network byte order. Since these functions are not standard, so was implemented custom_covectors.h file with the correct version of htonll (and similarly ntohll) to ensure portability and eliminate the warning.
    Technique:

    The struct is packed to ensure consistent size and alignment across platforms.
    Network byte order conversion ensures that the message format is correctly interpreted regardless of the endianness of the system.

    Why It Works:

    The fixed-size struct ensures that messages are consistently serialized and deserialized, making communication reliable.
    The use of uint16_t and uint64_t provides well-defined sizes for the fields, avoiding ambiguity.

3. Message Storage with Search by MessageId

    Implementation:
        Messages are stored in a custom hash map (CustomHashMap) defined in custom_hash_map.h.
        The hash map uses MessageId as the key and the entire Message struct as the value.
        Functions like hash_map_insert and hash_map_contains allow efficient insertion and lookup by MessageId.
    Technique:
        The hash map uses open addressing with linear probing to handle collisions.
        A simple hash function (key % capacity) maps MessageId to an index in the hash map’s array.
        The hash map dynamically resizes when the load factor exceeds a threshold (0.75), ensuring performance doesn’t degrade with many entries.
    Why It Works:
        The hash map provides average-case O(1) time complexity for lookups and insertions, making it efficient for searching by MessageId.
        The custom implementation avoids dependencies on STL/Boostlibraries, meeting the requirement to avoid those libraries.

4. Asynchronous TCP Transmission When MessageData == 10

    Implementation:
        A third thread (transmitterThread) in main.c monitors a transmit queue (transmitQueue) for messages to send via TCP.
        When a received message has MessageData == 10, it is added to the transmitQueue in receiverThread.
        The transmitterThread pops messages from the queue and submits them as tasks to a thread pool (ThreadPool) defined in thread_utils.h.
        The thread pool’s worker threads (asyncSendWorker) send the messages asynchronously over a TCP socket to the receiver on port 6000.
    Technique:
        A thread pool with a fixed number of workers (2 in this case) is used to handle TCP sends, avoiding the overhead of creating a new thread for each send.
        The thread pool uses a generic queue (CustomQueue) to store SendTask structs, which contain the socket descriptor and message to send.
        Condition variables (pthread_cond_t) are used to signal worker threads when new tasks are available, ensuring efficient task distribution.
    Why It Works:
        Asynchronous sending via a thread pool ensures that the transmitterThread isn’t blocked by the TCP send operation, allowing it to process the next message quickly.
        The thread pool reuses threads, reducing the overhead of thread creation and destruction, which is critical for performance in a system optimized for quick response.

5. Duplicate Filtering by MessageId

    Implementation:
        In receiverThread, before storing a message in the hash map, the thread checks if the MessageId already exists using hash_map_contains.
        If the MessageId is already present, the message is skipped, and a log message is printed (e.g., “Receiver X skipped duplicate ID=Y”).
        If the MessageId is not present, the message is inserted into the hash map and processed further.
    Technique:
        The hash map’s hash_map_contains function provides a fast lookup to detect duplicates.
        Thread safety is ensured by protecting access to the hash map with a mutex (mtxStore), as described in requirement 8.
    Why It Works:
        The hash map ensures that duplicates are detected efficiently, preventing redundant processing or transmission.
        The solution handles duplicates across both receiving threads, as the hash map is shared and thread-safe.

6. Main Target Platform: Linux

    Implementation:
        The project uses POSIX APIs for threads (pthread.h), sockets (sys/socket.h, arpa/inet.h), and non-blocking I/O (fcntl.h, sys/select.h), which are standard on Linux.
        The code avoids platform-specific features that wouldn’t work on Linux (e.g., Windows-specific APIs).
    Technique:
        POSIX threads provide a portable way to implement multithreading on Linux.
        Standard socket APIs ensure compatibility with Linux’s networking stack.
    Why It Works:
        Linux fully supports the POSIX standard, ensuring that the threading and socket operations work reliably.
        The project has been tested on a Linux environment.

7. CMake Build System
    The build process generates three executables: main, tcp_receiver, and udp_sender.

    Technique:

    CMake is used to manage the build, specifying the C11 standard and including all necessary files.

    Why It Works:

    CMake is a cross-platform build system widely used for C projects, making it easy to build the project on Linux.

8. Thread-Safe Access to the Message Container

    Implementation:
        The messageStore (hash map) is shared between the two receiving threads and is protected by a mutex (mtxStore).
        In receiverThread, access to the hash map is guarded by mutex_lock and mutex_unlock:
            mutex_lock(&mtxStore);
            if (!hash_map_contains(messageStore, msg.MessageId)) {
                hash_map_insert(messageStore, msg.MessageId, msg);
                // Process the message
            }
            mutex_unlock(&mtxStore);
        
        The transmitQueue is also shared between the receiving threads and the transmitting thread, protected by another mutex (mtxQueue).

    Technique:

        POSIX mutexes (pthread_mutex_t) are used for synchronization, wrapped in thread_utils.h functions (mutex_lock, mutex_unlock).
        A condition variable (cv) is used to signal the transmitterThread when new messages are added to the transmitQueue.

    Why It Works:

        The mutex ensures that only one thread can access the hash map or queue at a time, preventing data races.
        The condition variable allows the transmitterThread to wait efficiently for new messages, reducing CPU usage compared to busy-waiting.

9. Separate Applications for UDP Sender and TCP Receiver

    Implementation:
        The project includes three separate applications:
            udp_sender.c: Sends UDP messages to ports 5000 and 5001.
            main.c: Contains the two UDP receivers and the TCP transmitter.
            tcp_receiver.c: Listens for TCP messages on port 6000.
        These are built as separate executables (udp_sender, main, tcp_receiver) via CMake.
    Technique:
        Each application is self-contained, with its own main function, and communicates via sockets.
        The udp_sender sends 10 messages with varying MessageId and MessageData values, simulating a realistic workload.
        The tcp_receiver listens for incoming TCP connections and prints received messages.
    Why It Works:
        Separating the components into different applications makes the system modular and easier to test.
        It also mirrors a real-world scenario where the sender and receiver might be on different machines.

10. Non-Blocking Sockets
    Implementation:

    All sockets (UDP and TCP) are set to non-blocking mode using fcntl:

        fcntl(sock, F_SETFL, O_NONBLOCK);

    In receiverThread, transmitterThread, and tcp_receiver.c, the select system call is used to wait for socket events (e.g., data available to read, socket ready to write):

        FD_ZERO(&read_fds);
        FD_SET(sock, &read_fds);
        struct timeval tv = {0, 10000}; // 10ms timeout
        int ready = select(sock + 1, &read_fds, NULL, NULL, &tv);

        Technique:

            select allows the program to wait for multiple socket events efficiently, avoiding busy-waiting.
            A short timeout (10ms) ensures that the threads can periodically check the done flag to exit gracefully.

    Why It Works:

        Non-blocking sockets prevent the threads from hanging on I/O operations, which is critical for quick response times.
        select ensures that the program only processes sockets when they are ready, reducing CPU usage and improving responsiveness.

11. C/C++ Without STL/Boost

    Implementation:
        The project is written entirely in C/C++, using only standard C libraries (stdio.h, stdlib.h, etc.) and POSIX APIs.
        Custom data structures (CustomHashMap, CustomQueue) are implemented in custom_hash_map.h and custom_queue.h instead of using STL equivalents.
        No Boost libraries are used.
    Technique:
        The hash map (CustomHashMap) uses an array with open addressing for storage.
        The queue (CustomQueue) is a linked list-based implementation, made generic using void* pointers.
        Memory management is handled manually with malloc and free.
    Why It Works:
        The custom implementations provide the necessary functionality (hash map for lookup, queue for task management) without relying on STL or Boost.
        Manual memory management ensures full control over resources, which is important for a low-level system.

12. Optimize for Quick Response to Each Message

    Implementation:
        Non-Blocking Sockets with select: As mentioned, select is used to avoid blocking on socket operations, ensuring that threads can respond quickly to new messages.
        Thread Pool for TCP Sends: The ThreadPool in thread_utils.h uses a fixed number of worker threads to handle TCP sends asynchronously, reducing the overhead of thread creation.
        Efficient Synchronization: Mutexes are used sparingly, only when accessing shared data (messageStore, transmitQueue), and condition variables prevent busy-waiting.
        Generic Queue for Task Management: The CustomQueue in custom_queue.h is optimized for fast push and pop operations (O(1) time complexity) and is used for both the transmit queue and the thread pool’s task queue.
    Technique:
        Non-Blocking I/O: Using select ensures that the program only processes sockets when they are ready, avoiding delays from blocking calls.
        Thread Pool: Reusing threads for TCP sends eliminates the overhead of creating a new thread for each message, which can be significant in a high-throughput system.
        Efficient Data Structures: The hash map and queue are designed for fast operations (O(1) average case), minimizing the time spent on message lookup and task management.
        Reduced Synchronization Overhead: The use of condition variables (cond_wait, cond_signal) ensures that threads wait efficiently for new tasks or messages, rather than polling.
    Why It Works:
        The combination of non-blocking I/O, a thread pool, and efficient data structures ensures that the system can process and respond to messages with minimal latency.
        The select call with a short timeout allows the program to balance responsiveness with CPU efficiency, checking for new data frequently without busy-waiting.
        The thread pool ensures that TCP sends are handled in parallel, preventing the transmitterThread from being a bottleneck.

## Why the Solution Works

The solution works effectively because it addresses all requirements while optimizing for performance and reliability:

    Correctness:
        The system correctly receives UDP messages, filters duplicates, and transmits messages with MessageData == 10 via TCP.
        Thread safety is ensured through mutexes and condition variables, preventing data races and ensuring consistent behavior.
        The message format is well-defined and handled correctly with network byte order conversions.
    Performance:
        Non-blocking sockets and select minimize latency by ensuring that threads only process sockets when they are ready.
        The thread pool reduces the overhead of thread creation, allowing for quick and efficient TCP sends.
        The hash map and queue provide fast lookups and task management, critical for a system that needs to process messages quickly.
        Synchronization overhead is minimized by using condition variables and limiting mutex usage to necessary critical sections.
    Reliability:
        The project includes proper error handling via the logError function, which logs errors with errno details for debugging.
        Memory management is handled carefully, with dynamic allocations (malloc) paired with corresponding deallocations (free) to prevent leaks.
        The separate applications (udp_sender, tcp_receiver) make the system modular and easier to test.
    Portability:
        The use of POSIX APIs ensures that the project runs on Linux and other POSIX-compliant systems.
        CMake provides a portable build system, making it easy to compile the project on different Linux distributions.
    Maintainability:
        The code is organized into modular files (main.c, thread_utils.h, log_error.h, etc.), with clear separation of concerns.
        The README.md provides detailed instructions for building, running, and understanding the project.
        Custom data structures are well-documented and reusable for future extensions.

## Techniques Used for Optimization

The following techniques were critical for optimizing the system for quick response:

    Non-Blocking Sockets with select:
        Sockets are set to non-blocking mode, and select is used to wait for events, ensuring that threads don’t block on I/O operations.
        A 10ms timeout in select allows the program to check for termination conditions (done flag) without introducing significant delays.
    Thread Pool for Asynchronous Sends:
        A thread pool with 2 worker threads handles TCP sends, reusing threads to avoid the overhead of creating a new thread for each message.
        The thread pool uses a generic queue (CustomQueue) for task management, with condition variables to signal workers when new tasks are available.
    Efficient Data Structures:
        The CustomHashMap provides O(1) average-case lookups for duplicate filtering.
        The CustomQueue provides O(1) push and pop operations for task and message queuing.
    Minimized Synchronization Overhead:
        Mutexes are used only when necessary (e.g., accessing messageStore or transmitQueue), reducing contention.
        Condition variables prevent busy-waiting, allowing threads to wait efficiently for new messages or tasks.

## Final Code Summary

The project consists of the following key files (as described in the updated README.md):

    Source Files:
        main.c: Implements the two UDP receivers and the TCP transmitter.
        tcp_receiver.c: Implements the TCP receiver.
        udp_sender.c: Implements the UDP sender.
    Header Files:
        message.h: Defines the Message struct.
        custom_covectors.h Custom convector htonll (and similarly ntohll)
        custom_hash_map.h: Custom hash map for duplicate filtering.
        custom_queue.h: Generic queue for task and message management.
        custom_output.h: Custom output functions (print_out, print_err).
        thread_utils.h: Thread pool and synchronization utilities.
        log_error.h: Shared utility functions (logError).

    Build System:

    CMakeLists.txt: Configures the build process for the three executables.

## Conclusion

    Tthis project successfully meets all the specified requirements while optimizing for quick response to each message. The use of non-blocking sockets, a thread pool, efficient data structures, and minimal synchronization overhead ensures that the system is both fast and reliable. The modular design, with separate applications for the sender and receiver, makes the system easy to test and extend. The project is well-suited for a Linux environment, using POSIX APIs and a CMake build system for portability and ease of use.