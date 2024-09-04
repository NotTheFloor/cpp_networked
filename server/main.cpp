#include <cstdint>
#include <iostream>
#include <thread>
#include <memory>
#include <mutex>
#include <atomic>
#include <sys/socket.h>

#include "logger.h"
#include "event.h"
#include "ntwk.h"

void pushNetworkEvent(SharedNetResources &sharedNetResources, std::unique_ptr<BaseEvent>(event))
{
    std::lock_guard<std::mutex> lock(sharedNetResources.queueMutex);
    sharedNetResources.eventQueue.push(std::move(event));

    uint64_t u = 1;
    write(sharedNetResources.eventfd, &u, sizeof(uint64_t));
}

int main()
{
    bool mainRunning = true;
    SharedResources sharedResources;
    SharedNetResources sharedNetResources;
    std::atomic<bool> networkShutdownFlag(false);

    // Create network thread
    std::cout << "Creating network thread... ";
    Logger::getInstance().log(LogLevel::Info, "Creating network thread ...");
    std::thread network_thread(network_main, std::ref(sharedResources), std::ref(sharedNetResources), std::ref(networkShutdownFlag));
    std::cout << "created" << std::endl;

    while(mainRunning) {
        std::unique_ptr<BaseEvent> event;
        {
            std::unique_lock<std::mutex> lock(sharedResources.queueMutex);
            sharedResources.eventCondition.wait(lock, [&sharedResources]{ return !sharedResources.eventQueue.empty(); });
            event = std::move(sharedResources.eventQueue.front());
            sharedResources.eventQueue.pop();
        }

        switch(event->eventType) {
            case EventType::EventTypeMessage: {
                auto *eventData = static_cast<MessageEvent*>(event.get());
                std::cout << "Event loop data: " << eventData->message << std::endl;

                if (eventData->message == "/WOW") {
                    int socketId = eventData->clientId;
                    std::string strBuffer("BIGGER WOW");
                    auto event = std::make_unique<MessageEvent>(socketId, strBuffer);

                    pushNetworkEvent(std::ref(sharedNetResources), std::move(event));
                }
                break;
            }
            case EventType::ConnectReq: {
                auto *eventData = static_cast<ConnectReqEvent*>(event.get());
                Logger::getInstance().log(LogLevel::Info, "Connection request from: " + eventData->name);

                uint32_t clientAddr = eventData->clientAddr;
                auto event = std::make_unique<ConnectAcceptEvent>(1, clientAddr);

                pushNetworkEvent(std::ref(sharedNetResources), std::move(event));
                break;
            }
            case EventType::Shutdown:
                std::cout << "Setting nsf to true" << std::endl;
                networkShutdownFlag = true;
                mainRunning = false;
                break;
            case EventType::ClientDisconnect: {
                auto *eventData = static_cast<ClientDisconnectEvent*>(event.get());
                std::string discType;

                if (eventData->disconnectType == DisconnectType::Graceful)
                    discType = "graceful";
                else if (eventData->disconnectType == DisconnectType::Ungraceful)
                    discType = "ungraceful";
                else if (eventData->disconnectType == DisconnectType::Forced)
                    discType = "forced";
                else if (eventData->disconnectType == DisconnectType::Timeout)
                    discType = "timeout";
                        
                Logger::getInstance().log(LogLevel::Debug, "Client " + std::to_string(eventData->clientId) + " disconnected: " + discType);
                break;
            }
            default:
                Logger::getInstance().log(LogLevel::Warning, "Reached default switch statment in server main");
                break;
        }
    }

    std::cout << "Waiting for network thread to finish... " << std::endl;
    network_thread.join();

    std::cout << "finished" << std::endl;

    return 1;
}
