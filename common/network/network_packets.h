#pragma once

#include <netinet/in.h>
#include <functional>
#include <vector>
#include <iostream>
#include <cstdint>

#include "network/packets.h"
#include "event.h"

#define UDP_RECV_PKT_SIZE 2048

struct SharedNetResources : SharedResources {
   int eventfd;
};

struct TcpPcktHeader {
    uint16_t type;
    uint16_t length;
};

struct UdpPcktHeader {
    uint16_t type;
    uint16_t length;
    uint16_t sequence;
    uint16_t checksum;
    //uint16_t flags;
};

void pushNetworkEvent(SharedNetResources &sharedNetResources, std::unique_ptr<BaseEvent>(event));

int setnonblocking(int sock);
uint16_t calcChecksum(const std::vector<uint8_t> &payload);
std::unique_ptr<BasePacket> packetFactory(uint16_t type);
void sendTCPPacket(int sock, uint16_t type, const BasePacket & basePacket);
std::unique_ptr<BasePacket> recvTCPPacket(int sock, std::function<std::unique_ptr<BasePacket>(uint16_t)> packetFactory);
void sendUDPPacket(int sock, const sockaddr_in &destAddr, uint16_t type, const BasePacket &basePacket, uint16_t sequence = 0);
std::unique_ptr<BasePacket> recvUDPPacket(int sock, std::function<std::unique_ptr<BasePacket>(uint16_t)> packetFactory, sockaddr_in &remoteAddr);

