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
    ConnectAccept
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

    ConnectReqEvent(uint32_t id) : clientAddr(id) {
        eventType = EventType::ConnectReq;
    }
};

struct ConnectAcceptEvent : BaseEvent {
    int clientId;

    ConnectAcceptEvent(int id) : clientId(id) {
        eventType = EventType::ConnectAccept;
    }
};
