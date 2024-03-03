# MCP2515_Docker_1

How to use:
1. Pull docker image from docker hub:
   docker pull thainh5/mcp2515
2. Enable can with baudrate and loopback mode:
   sudo /sbin/ip link set can0 up type can bitrate 500000 loopback on
3. Open new terminal listen can message:
   candump can0 -n1
5. Open other terminal send can message:
   docker run --rm --privileged -v /dev:/dev --network host thainh5/mcp2515:latest
   
