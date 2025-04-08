#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <errno.h>
#include "../utils/custom_convectors.h"
#include "../utils/custom_output.h"
#include "../utils/log_error.h"
#include "../utils/message.h"

int main() {
    // Create TCP socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        logError("Socket creation failed");
        return 1;
    }
    fcntl(sock, F_SETFL, O_NONBLOCK);  // Set non-blocking mode

    // Bind to port 6000
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(6000);
    addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        logError("Bind failed");
        close(sock);
        return 1;
    }
    if (listen(sock, 5) < 0) {
        logError("Listen failed");
        close(sock);
        return 1;
    }

    print_out("TCP receiver listening on port 6000...\n");

    // Accept client connection
    int clientSock = -1;
    fd_set read_fds;
    struct timeval tv;
    while (clientSock < 0) {
        FD_ZERO(&read_fds);
        FD_SET(sock, &read_fds);
        tv.tv_sec = 0;
        tv.tv_usec = 10000;  // 10ms timeout

        int ready = select(sock + 1, &read_fds, NULL, NULL, &tv);
        if (ready < 0) {
            logError("Accept select failed");
            close(sock);
            return 1;
        }
        if (ready == 0) {
            continue;
        }
        if (FD_ISSET(sock, &read_fds)) {
            clientSock = accept(sock, NULL, NULL);
            if (clientSock < 0 && errno != EAGAIN) {
                logError("Accept failed");
                close(sock);
                return 1;
            }
        }
    }
    fcntl(clientSock, F_SETFL, O_NONBLOCK);

    // Receive messages
    char buffer[sizeof(Message)];
    while (1) {
        FD_ZERO(&read_fds);
        FD_SET(clientSock, &read_fds);
        tv.tv_sec = 0;
        tv.tv_usec = 10000;  // 10ms timeout

        int ready = select(clientSock + 1, &read_fds, NULL, NULL, &tv);
        if (ready < 0) {
            logError("Recv select failed");
            break;
        }
        if (ready == 0) {
            continue;
        }
        if (FD_ISSET(clientSock, &read_fds)) {
            int bytes = recv(clientSock, buffer, sizeof(Message), 0);
            if (bytes == sizeof(Message)) {
                Message msg;
                memcpy(&msg, buffer, sizeof(Message));
                msg.MessageSize = ntohs(msg.MessageSize);
                msg.MessageId = ntohll(msg.MessageId);
                msg.MessageData = ntohll(msg.MessageData);
                char outBuffer[256];
                snprintf(outBuffer, sizeof(outBuffer), "Received via TCP: ID=%lu, Data=%lu\n", msg.MessageId, msg.MessageData);
                print_out(outBuffer);
            } else if (bytes < 0 && errno != EAGAIN) {
                logError("Recv failed");
                break;
            } else if (bytes == 0) {
                print_out("Client disconnected\n");
                break;
            }
        }
    }

    close(clientSock);
    close(sock);
    return 0;
}
