#include "network/network_packets.h"
#include "logger.h"
#include <cstdint>

uint16_t calcChecksum(const std::vector<uint8_t> &payload) {
    uint32_t sum = 0;

    for (size_t i = 0; i < payload.size() - 1; i += 2) {
        uint16_t word = (payload[i] << 8) | payload[i + 1];
        sum += word;

        if (sum > 0xFFFF)
            sum = (sum & 0xFFFF) + (sum >> 16);
    }

    if (payload.size() % 2 != 0) {
        uint16_t word = payload.back() << 8;
        sum += word;

        if (sum > 0xFFFF)
            sum = (sum & 0xFFFF) + (sum >> 16);
    }

    uint16_t checksum = static_cast<uint16_t>(~sum & 0xFFFF);

    return checksum;
}

std::unique_ptr<BasePacket> packetFactory(uint16_t type)
{
    switch (type) {
        case CONN_REQ_PACKET_ID: // Need to define message types somewhere still
            Logger::getInstance().log(LogLevel::Debug, "CONN_REQ");
            return std::make_unique<ConnReqPacket>();
        case MESSAGE_PACKET_ID :
            Logger::getInstance().log(LogLevel::Debug, "MESSAGE");
            return std::make_unique<MessagePacket>();
        case DISC_CLI_PACKET_ID:
            Logger::getInstance().log(LogLevel::Debug, "DISC_CLI");
            return std::make_unique<DisconnectClientPacket>();
        default:
            return nullptr;
    }
}

void sendUDPPacket(int sock, const sockaddr_in &destAddr, uint16_t type, const BasePacket &basePacket, uint16_t sequence = 0)
{
    std::vector<uint8_t> payload = basePacket.serialize();
    uint16_t payload_length = static_cast<uint16_t>(payload.size());
    uint16_t checksum = calcChecksum(payload);

    UdpPcktHeader header = { htons(type), htons(payload_length), htons(sequence), htons(checksum) };

    std::vector<uint8_t> buffer(sizeof(header) + payload.size());

    std::memcpy(buffer.data(), &header, sizeof(header));
    std::memcpy(buffer.data() + sizeof(header), payload.data(), payload.size());

    sendto(sock, buffer.data(), buffer.size(), 0, (struct sockaddr*)&destAddr, sizeof(destAddr));
}

void sendTCPPacket(int sock, uint16_t type, const BasePacket &basePacket)
{
    std::vector<uint8_t> payload = basePacket.serialize();
    uint16_t payload_length = static_cast<uint16_t>(payload.size());

    TcpPcktHeader header = { htons(type), htons(payload_length) };

    send(sock, &header, sizeof(header), 0);
    send(sock, payload.data(), payload_length, 0);
}

std::unique_ptr<BasePacket> recvTCPPacket(int sock, std::function<std::unique_ptr<BasePacket> (uint16_t)> packetFactory)
{
    TcpPcktHeader header;

    ssize_t bytesReceived = recv(sock, &header, sizeof(header), 0);

    if (bytesReceived == 0)
    {
        Logger::getInstance().log(LogLevel::Warning, "Client disconnected");
        return nullptr;
    } else if (bytesReceived < 0) {
        Logger::getInstance().log(LogLevel::Error, std::string("Error receiving data: ") + strerror(errno));
        return nullptr;
    }

    uint16_t type = ntohs(header.type);
    uint16_t length = ntohs(header.length);

    std::vector<uint8_t> payload(length);
    bytesReceived = recv(sock, payload.data(), length, 0);

    if (bytesReceived == 0)
    {
        Logger::getInstance().log(LogLevel::Warning, "Client disconnected");
        return nullptr;
    } else if (bytesReceived < 0) {
        Logger::getInstance().log(LogLevel::Error, std::string("Error receiving data: ") + strerror(errno));
        return nullptr;
    }

    std::unique_ptr<BasePacket> basePacket = packetFactory(type);
    if (basePacket) {
        basePacket->deserialize(payload);
    }

    return basePacket;
}
