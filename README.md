# MCP2515_Docker_1

How to use:
1. Pull docker image from docker hub:
   docker pull thainh5/mcp2515
2. Enable can with baudrate and loopback mode:
   sudo /sbin/ip link set can0 up type can bitrate 500000 loopback on
3. Open new terminal listen can message:
   candump can0 -n1
4. Open other terminal send can message:
   docker run --rm --privileged -v /dev:/dev --network host thainh5/mcp2515:latest

# USB CAN
1. Find USB interface:
   ls /dev/ttyUSB*
2. Attach slcan (suppose USB0 is available):
   sudo slcan_attach /dev/ttyUSB0
3. Create a CAN network interface from a serial line device
   sudo slcand ttyUSB0 slcan0
4. Link up can network
   sudo ip link set slcan0 up
5. Use similar code in MCP2515_Docker_1 with replacement can0 -> slcan0
   
