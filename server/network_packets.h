#pragma once

#include <netinet/in.h>
#include <functional>

#include "packets.h"

struct TcpPcktHeader {
    uint16_t type;
    uint16_t length;
};

std::unique_ptr<BasePacket> packetFactory(uint16_t type);
void sendPacket(int sock, uint16_t type, const BasePacket & basePacket);
std::unique_ptr<BasePacket> recvPacket(int sock, std::function<std::unique_ptr<BasePacket>(uint16_t)> packetFactory);
