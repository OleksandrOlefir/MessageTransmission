#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <errno.h>
#include "../utils/custom_convectors.h"
#include "../utils/custom_output.h"
#include "../utils/log_error.h"
#include "../utils/message.h"

/* Send a message over UDP */
void sendMessage(int sock, struct sockaddr_in* addr, Message msg) {
    Message netMsg = msg;
    netMsg.MessageSize = htons(msg.MessageSize);
    netMsg.MessageId = htonll(msg.MessageId);
    netMsg.MessageData = htonll(msg.MessageData);
    if (sendto(sock, &netMsg, sizeof(Message), 0, (struct sockaddr*)addr, sizeof(*addr)) < 0) {
        char buffer[256];
        snprintf(buffer, sizeof(buffer), "Failed to send message ID=%lu", msg.MessageId);
        logError(buffer);
    }
}

int main() {
    // Create UDP socket
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        logError("Socket creation failed");
        return 1;
    }

    // Set up destination addresses
    struct sockaddr_in addr1, addr2;
    addr1.sin_family = AF_INET;
    addr1.sin_port = htons(5000);
    inet_pton(AF_INET, "127.0.0.1", &addr1.sin_addr);
    addr2.sin_family = AF_INET;
    addr2.sin_port = htons(5001);
    inet_pton(AF_INET, "127.0.0.1", &addr2.sin_addr);

    // Send test messages
    for (int i = 0; i < 10; ++i) {
        Message msg = {sizeof(Message), 1, (uint64_t)(i % 5), (i % 3 == 0) ? 10 : i};
        sendMessage(sock, &addr1, msg);
        sendMessage(sock, &addr2, msg);
        char buffer[256];
        snprintf(buffer, sizeof(buffer), "Sent: ID=%lu, Data=%lu\n", msg.MessageId, msg.MessageData);
        print_out(buffer);
        usleep(500000);  // Delay between sends
    }

    close(sock);
    return 0;
}