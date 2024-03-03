#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/can.h>
#include <linux/can/raw.h>

int main() {
    const char *ifname = "can0";

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
    ioctl(sock, SIOCGIFINDEX, &ifr);
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    // Bind the socket to the CAN interface
    if (bind(sock, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) == -1) {
        perror("bind");
        close(sock);
        return 1;
    }

    // Send a CAN frame
    struct can_frame frame;
    frame.can_id = 0x123;
    frame.can_dlc = 2;
    frame.data[0] = 0xAA;
    frame.data[1] = 0xBB;

    ssize_t nbytes = write(sock, &frame, sizeof(struct can_frame));
    if (nbytes == -1) {
        perror("write");
        close(sock);
        return 1;
    }

    std::cout << "Sent " << nbytes << " bytes" << std::endl;

    // Close the socket
    close(sock);

    return 0;
}
