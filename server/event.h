#pragma once

#include <string>
#include <queue>
#include <memory>
#include <mutex>
#include <condition_variable>

enum class EventType {
    EventTypeMessage,
    EventTypeMessage2
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

struct MessageEvent2 : BaseEvent {
    int clientId;
    std::string message;

    MessageEvent2(int id, const std::string &message) : clientId(id), message(message) {
        eventType = EventType::EventTypeMessage2;
    }
};
