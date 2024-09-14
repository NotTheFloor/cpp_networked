#include "network/network_packets.h"
#include "logger.h"
#include "network/packets.h"
#include <cstdint>
#include <fcntl.h>

// Provided by selbie on stackoverflow
int setnonblocking(int sock)
{
    int result;
    int flags;

    flags = ::fcntl(sock, F_GETFL, 0);

    if (flags == -1)
    {
        return -1;  // error
    }

    flags |= O_NONBLOCK;

    result = fcntl(sock , F_SETFL , flags);

    return result;
}

// Should be moved outside of network related code and imported
void pushEvent(SharedResources &sharedResources, std::unique_ptr<BaseEvent>(event))
{
    std::lock_guard<std::mutex> lock(sharedResources.queueMutex);
    sharedResources.eventQueue.push(std::move(event));

    sharedResources.eventCondition.notify_one();
}

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

void sendUDPPacket(int sock, const sockaddr_in &destAddr, uint16_t type, const BasePacket &basePacket, uint16_t sequence)
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


std::unique_ptr<BasePacket> recvUDPPacket(int sock, std::function<std::unique_ptr<BasePacket>(uint16_t)> packetFactory, sockaddr_in &remoteAddr) {
    UdpPcktHeader header;
    socklen_t addr_len = sizeof(remoteAddr);

    uint8_t buffer[UDP_RECV_PKT_SIZE];

    ssize_t bytesReceived = recvfrom(sock, buffer, sizeof(buffer), 0, (struct sockaddr*)&remoteAddr, &addr_len);
    if (bytesReceived < sizeof(UdpPcktHeader)) {
        // Need to handle
        Logger::getInstance().log(LogLevel::Error, "Received less than header");
        return nullptr;
    }

    std::memcpy(&header, buffer, sizeof(header));
    uint16_t type = ntohs(header.type);
    uint16_t length = ntohs(header.length);
    uint16_t sequence = ntohs(header.sequence);
    uint16_t checksum = ntohs(header.checksum);

    if (bytesReceived < sizeof(header) + length) {
        Logger::getInstance().log(LogLevel::Error, "Receieved less than header spec'd length");
        return nullptr;
    }

    std::vector<uint8_t> payload(length);
    std::memcpy(payload.data(), buffer + sizeof(header), length);

    uint16_t recvChecksum = calcChecksum(payload);
    if (checksum != recvChecksum) {
        Logger::getInstance().log(LogLevel::Error, "Check sums do not match");
        return nullptr;
    }
    
    std::unique_ptr<BasePacket> basePacket = packetFactory(type);

    if (basePacket)
        basePacket->deserialize(payload);

    return basePacket;
}


