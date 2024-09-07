#include "network/network_packets.h"
#include "logger.h"

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

void sendPacket(int sock, uint16_t type, const BasePacket &basePacket)
{
    std::vector<uint8_t> payload = basePacket.serialize();
    uint16_t payload_length = static_cast<uint16_t>(payload.size());

    TcpPcktHeader header = { htons(type), htons(payload_length) };

    send(sock, &header, sizeof(header), 0);
    send(sock, payload.data(), payload_length, 0);
}

std::unique_ptr<BasePacket> recvPacket(int sock, std::function<std::unique_ptr<BasePacket> (uint16_t)> packetFactory)
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
