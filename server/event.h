#pragma once

#include <string>
#include <queue>
#include <memory>
#include <mutex>
#include <condition_variable>

enum class EventType {
    EventTypeMessage,
    Shutdown,
    ConnectReq,
    ConnectAccept,
    ClientDisconnect
};

enum class DisconnectType {
    Graceful,
    Timeout,
    Ungraceful,
    Forced
};

struct BaseEvent {
    EventType eventType;
    virtual ~BaseEvent() = default;
};

struct SharedResources {
    std::queue<std::unique_ptr<BaseEvent>> eventQueue;
    std::mutex queueMutex;
    std::condition_variable eventCondition;
};

struct MessageEvent : BaseEvent {
    int clientId;
    std::string message;

    MessageEvent(int id, const std::string &message) : clientId(id), message(message) {
        eventType = EventType::EventTypeMessage;
    }
};

struct ShutdownEvent : BaseEvent {
    int clientId;

    ShutdownEvent(int id) : clientId(id) {
        eventType = EventType::Shutdown;
    }
};

struct ConnectReqEvent : BaseEvent {
    uint32_t clientAddr;
    std::string name;

    ConnectReqEvent(uint32_t id, std::string name) : clientAddr(id), name(name) {
        eventType = EventType::ConnectReq;
    }
};

struct ConnectAcceptEvent : BaseEvent {
    int clientId;
    uint32_t clientAddr;

    ConnectAcceptEvent(int id, uint32_t cAddr) : clientId(id), clientAddr(cAddr) {
        eventType = EventType::ConnectAccept;
    }
};

struct ClientDisconnectEvent : BaseEvent {
    int clientId;
    DisconnectType disconnectType;

    ClientDisconnectEvent(int id, DisconnectType dt) : clientId(id), disconnectType(dt) {
        eventType = EventType::ClientDisconnect;
    }
};
