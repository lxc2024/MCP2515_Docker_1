# MCP2515_Docker_1

How to use:
1. Pull docker image from docker hub:
   docker pull thainh5/mcp2515
2. Enable can with baudrate and loopback mode:
   sudo /sbin/ip link set can0 up type can bitrate 500000 loopback on
   OR
   sudo ip link set can0 up type can bitrate 500000
4. Open new terminal listen can message:
   candump can0 -n1
5. Open other terminal send can message:
   docker run --rm --privileged -v /dev:/dev --network host thainh5/mcp2515:latest

# USB CAN
1. Run container multi-platorm:
   docker run -it --rm --privileged -v /dev:/dev --network host thainh5/can_usb bash
2. Run multi container with docker-compose
   docker-compose run --rm --service-ports my_docker_ubuntu bash
   docker-compose run --rm --service-ports can_usb bash
   
