#pragma once

#include <vector>
#include <string>
#include <memory>
#include <netinet/in.h>
#include <cstring>

#define CONN_REQ_PACKET_ID 0x0001
#define MESSAGE_PACKET_ID 0x0002

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
