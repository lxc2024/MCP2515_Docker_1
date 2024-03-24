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
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <termios.h>
#include <fcntl.h>

// Global variables for synchronization
volatile sig_atomic_t exitRequested = 0;
std::mutex mtx;
std::condition_variable cv;

// Function to handle Ctrl+C signal
void signalHandler(int signal) {
    if (signal == SIGINT) {
        std::cout << "Ctrl+C received. Exiting..." << std::endl;
        exitRequested = 1;
        cv.notify_all(); // Notify all threads waiting on the condition variable
    }
}

// Function to send CAN messages periodically
void sendMessages(int sock) {
    while (!exitRequested) {
        // Send a CAN frame
        struct can_frame sendFrame;
        sendFrame.can_id = 0x123;
        sendFrame.can_dlc = 2;
        sendFrame.data[0] = 0xAA;
        sendFrame.data[1] = 0xBB;

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
void receiveMessages(int sock) {
    while (!exitRequested) {
        // Set up fd_set for select
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(sock, &readfds);

        // Set up timeout for select (100 ms)
        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 100000;

        // Wait for data to be available on the socket or for exit signal
        int ret = select(sock + 1, &readfds, nullptr, nullptr, &timeout);
        if (ret < 0) {
            perror("select");
            break;
        } else if (ret == 0) {
            // Timeout occurred, continue waiting
            continue;
        }

        // Receive a CAN frame
        struct can_frame recvFrame;
        ssize_t recvBytes = read(sock, &recvFrame, sizeof(struct can_frame));
        if (recvBytes == -1) {
            perror("read");
            break;
        }

        if (recvBytes > 0) {
            std::cout << "Received " << recvBytes << " bytes" << std::endl;
            std::cout << "CAN ID: 0x" << std::hex << recvFrame.can_id << std::dec << std::endl;
            std::cout << "Data: ";
            for (int i = 0; i < recvFrame.can_dlc; ++i) {
                std::cout << std::hex << static_cast<int>(recvFrame.data[i]) << " ";
            }
            std::cout << std::dec << std::endl;
			
			if (recvFrame.can_id == 0x111) {
				sendFrame.can_id = 0x601;
				sendFrame.can_dlc = 8; // Number of data bytes
				sendFrame.data[0] = 0x02;
				sendFrame.data[1] = 0x10;
				sendFrame.data[2] = 0x01;
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

				std::cout << "Sent Diag Message" << std::endl;
			}

        }
    }
}

// Function to send CAN message when 'q' is pressed
void sendOnKeyPress(int sock) {
    // Set terminal to non-blocking mode
    struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    char input;
    while (!exitRequested) {
        // Check for user input
        int result = read(STDIN_FILENO, &input, 1);
        if (result > 0 && input == 'q') {
            // Send a CAN frame
            sendFrame.can_id = 0x601;
			sendFrame.can_dlc = 8; // Number of data bytes
			sendFrame.data[0] = 0x02;
			sendFrame.data[1] = 0x10;
			sendFrame.data[2] = 0x01;
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
        }
        // Sleep for a short duration to prevent high CPU usage
        usleep(10000);
    }

    // Restore terminal settings
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
}

int main() {
    const char *ifname = "can0";

    // Set up signal handler for Ctrl+C
    std::signal(SIGINT, signalHandler);

    // Create a socket for CAN communication
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

    // Create threads for sending and receiving CAN messages
	//std::thread senderThread(sendMessages, sock);
    std::thread receiverThread(receiveMessages, sock);
	std::thread keyPressThread(sendOnKeyPress, sock);

    // Wait for Ctrl+C to exit
    while (!exitRequested) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Join the receiver thread and exit
	//senderThread.join();
    receiverThread.join();
	keyPressThread.join();
	
    close(sock);

    return 0;
}
