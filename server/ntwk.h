#pragma once

#include <string>
#include <vector>
#include <atomic>
#include <cstring>
#include <netinet/in.h>
#include <memory>

#include "event.h"

struct SharedNetResources : SharedResources {
   int eventfd;
};

struct TcpPcktHeader {
    uint16_t type;
    uint16_t length;
};

class BasePacket {
    public:
        virtual ~BasePacket() = default;
        virtual std::vector<uint8_t> serialize() const = 0;
        virtual void deserialize(const std::vector<uint8_t> &data) = 0;
};

class MessagePacket : public BasePacket {
    public:
        std::string text;

        std::vector<uint8_t> serialize() const override {
            std::vector<uint8_t> data(text.begin(), text.end());
            return data;
        }

        void deserialize(const std::vector<uint8_t> &data) override {
            text = std::string(data.begin(), data.end());
        }
};

class ConnReqPacket : public BasePacket {
    public:
        std::string name;
        int placeHolder;

        std::vector<uint8_t> serialize() const override {
            std::vector<uint8_t> data(sizeof(int));

            std::memcpy(data.data(), &placeHolder, sizeof(int));

            uint16_t nameLength = static_cast<uint16_t>(name.size());
            nameLength = htons(nameLength);
            data.insert(data.end(), reinterpret_cast<uint8_t*>(&nameLength), reinterpret_cast<uint8_t*>(&nameLength) + sizeof(nameLength));
            data.insert(data.end(), name.begin(), name.end());

            return data;
        }

        void deserialize(const std::vector<uint8_t> &data) override {
            std::memcpy(&placeHolder, data.data(), sizeof(int));

            uint16_t nameLength;
            std::memcpy(&nameLength, data.data() + sizeof(int), sizeof(uint16_t));
            nameLength = ntohs(nameLength);
            name = std::string(data.data() + sizeof(int) + sizeof(uint16_t), data.data() + sizeof(int) + sizeof(uint16_t) + nameLength);
        }
};

std::unique_ptr<BasePacket> packetFactory(uint16_t type);
void sendPacket(int sock, uint16_t type, const BasePacket & basePacket);
std:unique_ptr<BasePacket> recvPacket(int sock, std::function<std::unique_ptr<BasePacket>(uint16_t)> packetFactory);
int network_main(SharedResources &sharedResources, SharedNetResources &sharedNetResources, std::atomic<bool> &shutdownFlag);

