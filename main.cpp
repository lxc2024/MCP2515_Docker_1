#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <thread>
#include <csignal>

// Global variable to indicate whether to exit the while loops
volatile sig_atomic_t exitRequested = 0;

// Signal handler for Ctrl+C (SIGINT)
static void signalHandler(int signal) {
    if (signal == SIGINT) {
        std::cout << "Ctrl+C received. Exiting..." << std::endl;
        exitRequested = 1;
    }
}

// Function to send CAN messages periodically
static void sendIGNMessages(int sock) {
    while (!exitRequested) {
        // Send a CAN frame F200000000000000
        struct can_frame sendFrame;
        sendFrame.can_id = 0x18DA92F1 | CAN_EFF_FLAG; // Extended identifier flag
		sendFrame.can_dlc = 8; // Number of data bytes
		sendFrame.data[0] = 0xF2;
		sendFrame.data[1] = 0x00;
		sendFrame.data[2] = 0x00;
		sendFrame.data[3] = 0x00;
		sendFrame.data[4] = 0x00;
		sendFrame.data[5] = 0x00;
		sendFrame.data[6] = 0x00;
		sendFrame.data[7] = 0x00;

        ssize_t sendBytes = write(sock, &sendFrame, sizeof(struct can_frame));
        if (sendBytes == -1) {
            perror("write");
            close(sock);
            return;
        }

        std::cout << "Sent " << sendBytes << " bytes" << std::endl;

        // Sleep for 100 milliseconds
        usleep(1000000);
    }
}

// Function to receive CAN messages continuously
static void receiveMessages(int sock) {
    while (!exitRequested) {
        // Receive a CAN frame
        struct can_frame recvFrame;
        ssize_t recvBytes = read(sock, &recvFrame, sizeof(struct can_frame));
        if (recvBytes == -1) {
            perror("read");
            close(sock);
            return;
        }

        if (recvBytes > 0) {
            std::cout << "Received " << recvBytes << " bytes" << std::endl;
            if (recvFrame.can_id & CAN_EFF_FLAG) {
                std::cout << "Extended CAN ID: 0x" << std::hex << (recvFrame.can_id & CAN_EFF_MASK) << std::dec << std::endl;
            } else {
                std::cout << "Standard CAN ID: 0x" << std::hex << recvFrame.can_id << std::dec << std::endl;
            }
            //std::cout << "CAN ID: 0x" << std::hex << recvFrame.can_id << std::dec << std::endl;
            std::cout << "Data: ";
            for (int i = 0; i < recvFrame.can_dlc; ++i) {
                std::cout << std::hex << static_cast<int>(recvFrame.data[i]) << " ";
            }
            std::cout << std::dec << std::endl;
        }
    }
}

int main() {
    const char *ifname = "can0";

    // Set up the signal handler for Ctrl+C (SIGINT)
    std::signal(SIGINT, signalHandler);

    // Create a socket
    int sock = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (sock == -1) {
        perror("socket");
        return 1;
    }

    // Specify the CAN interface
    struct sockaddr_can addr;
    struct ifreq ifr;
    std::strcpy(ifr.ifr_name, ifname);
    if (ioctl(sock, SIOCGIFINDEX, &ifr) == -1) {
        perror("ioctl");
        close(sock);
        return 1;
    }

    // Set up sockaddr_can structure
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    // Bind the socket to the CAN interface
    if (bind(sock, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) == -1) {
        perror("bind");
        close(sock);
        return 1;
    }

    // Create two threads, one for sending messages and one for receiving messages
    std::thread senderThread(sendIGNMessages, sock);
    std::thread receiverThread(receiveMessages, sock);

    // Join the threads (wait for them to finish, although they are infinite loops)
    senderThread.join();
    receiverThread.join();

    // Close the socket (this part is unreachable as the threads are infinite loops)
    close(sock);

    return 0;
}
