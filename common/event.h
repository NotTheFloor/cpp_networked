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
    ClientDisconnect,
    TimerFired
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

struct TimerFiredEvent : BaseEvent {
    std::string timerName;

    TimerFiredEvent(std::string tn) : timerName(tn) {
        eventType = EventType::TimerFired;
    }
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
    uint32_t uniqueId;
    std::string name;
    std::string ipAddr;
    std::string port;

    ConnectReqEvent(uint32_t id, std::string name, std::string ip, std::string port) : uniqueId(id), name(name), ipAddr(ip), port(port) {
        eventType = EventType::ConnectReq;
    }
};

struct ConnectAcceptEvent : BaseEvent {
    int clientId;
    uint32_t uniqueId;

    ConnectAcceptEvent(int id, uint32_t uid) : clientId(id), uniqueId(uid) {
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

void pushEvent(SharedResources &sharedResources, std::unique_ptr<BaseEvent>(event));

