#include <algorithm>
#include <cstdint>
#include <functional>
#include <iostream>
#include <sys/socket.h>
#include <sys/eventfd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <cstring>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <string>
#include <errno.h>

#include "logger.h"
#include "ntwk.h"
#include "event.h"

#define LISTEN_BACKLOG 5
#define MAX_EVENTS 10
#define MAX_CONN_ATTEMPTS 3


void pushEvent(SharedResources &sharedResources, std::unique_ptr<BaseEvent>(event))
{
    std::lock_guard<std::mutex> lock(sharedResources.queueMutex);
    sharedResources.eventQueue.push(std::move(event));

    sharedResources.eventCondition.notify_one();
}

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

// We start swapping to camelCase here... fine
int network_main(SharedResources &sharedResources, SharedNetResources &sharedNetResources, std::atomic<bool> &shutdownFlag)
{
    Logger::getInstance().log(LogLevel::Debug, "Starting network thread");

    // Note: we'll use contain which is C++20
    std::unordered_map<in_addr_t, int> pendingUserMap, connUserMap;
    sharedNetResources.eventfd = eventfd(0, 0);
    int _eventfd = sharedNetResources.eventfd;

    struct epoll_event ev, events[MAX_EVENTS];
    int conn_sock, nfds, epollfd;

    int list_sock = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);

    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(8080);
    serverAddress.sin_addr.s_addr = INADDR_ANY;

    sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    if (bind(list_sock, reinterpret_cast<struct sockaddr *>(&serverAddress), sizeof(serverAddress)) == -1)
    {
        Logger::getInstance().log(LogLevel::Error, "Error binding server socket");
        return 1;
    }

    listen(list_sock, LISTEN_BACKLOG);

    epollfd = epoll_create1(0); // Essentially same as create
    if (epollfd == -1)
    {
        Logger::getInstance().log(LogLevel::Error, "Error creating epoll file descriptor");
        return 1;
    }

    ev.events = EPOLLIN;
    ev.data.fd = list_sock;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, list_sock, &ev) == -1)
    {
        Logger::getInstance().log(LogLevel::Error, "Error epoll_ctl");
        return 1;
    }

    ev.events = EPOLLIN;
    ev.data.fd = _eventfd;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, _eventfd, &ev) == -1)
    {
        Logger::getInstance().log(LogLevel::Error, "Error epoll_ctl");
        return 1;
    }

    // Main network loop
    for(;;)
    {
        if (shutdownFlag == true)
            break;

        // Blocking function - wakes for network and eventfd fd's
        nfds = epoll_wait(epollfd, events, MAX_EVENTS, 0);
        if (nfds == -1)
        {
            Logger::getInstance().log(LogLevel::Error, "Error epoll_wait");
            return 1;
        }

        for(int n = 0; n < nfds; ++n)
        {
            // Listening socket active, likely connection attempt
            if (events[n].data.fd == list_sock)
            {
                conn_sock = accept(list_sock, (struct sockaddr*) &client_addr, &client_addr_len);
                if (conn_sock == -1) {
                    Logger::getInstance().log(LogLevel::Error, "Error accepting client");
                    return 1;
                }

                Logger::getInstance().log(LogLevel::Info, std::string("Client connected from: ") + inet_ntoa(client_addr.sin_addr));

                setnonblocking(conn_sock);
                ev.events = EPOLLIN | EPOLLHUP | EPOLLRDHUP;
                ev.data.fd = conn_sock;

                if (epoll_ctl(epollfd, EPOLL_CTL_ADD, conn_sock, &ev) == -1)
                {
                    Logger::getInstance().log(LogLevel::Error, "Failed to add client to epoll");
                    return 1;
                }


                if (pendingUserMap.contains(client_addr.sin_addr.s_addr)) {
                    pendingUserMap[client_addr.sin_addr.s_addr] += 1;
                    Logger::getInstance().log(LogLevel::Warning, "Client attempted while still pending");
                } else {
                    pendingUserMap[client_addr.sin_addr.s_addr] = 1;
                }

                // Pending event loop
            } else if (events[n].data.fd == _eventfd) {
                uint64_t u;
                read(_eventfd, &u, sizeof(uint64_t));

                std::unique_ptr<BaseEvent> event;
                {
                    std::lock_guard lock(sharedNetResources.queueMutex);
                    event = std::move(sharedNetResources.eventQueue.front());
                    sharedNetResources.eventQueue.pop();
                }

                // really should not happen - possible if eventfd remains active after
                // events already been handled and list is empty
                if (event == nullptr) {
                    Logger::getInstance().log(LogLevel::Warning, "sharedNetResources returned event is null");
                    continue;
                }

                switch(event->eventType) {
                    case EventType::ConnectAccept: {
                        auto *eventData = static_cast<ConnectAcceptEvent*>(event.get());

                        // Also need to test if user is already connected
                        if (!pendingUserMap.contains(client_addr.sin_addr.s_addr)) {
                            Logger::getInstance().log(LogLevel::Warning, "Accepted connection for non-pending user");
                            // Needs to be hadnled
                            break;
                        }

                        pendingUserMap.erase(eventData->clientAddr);
                        connUserMap[eventData->clientAddr] = eventData->clientId;
                        
                        Logger::getInstance().log(LogLevel::Debug, std::string("Connection request accepted with id: ") + std::to_string(eventData->clientId));
                        break;
                    }
                    case EventType::EventTypeMessage: {
                        auto *eventData = static_cast<MessageEvent*>(event.get());
                        break;
                    }
                    default:
                        Logger::getInstance().log(LogLevel::Error, "Default case");
                        break;
                }

            } else {
                std::unique_ptr<BasePacket> packetRecvd = recvPacket(events[n].data.fd, packetFactory);

                if (packetRecvd == nullptr) 
                {
                    int s2c = events[n].data.fd;

                    Logger::getInstance().log(LogLevel::Info, "Client socket closing");

                    if (epoll_ctl(epollfd, EPOLL_CTL_DEL, s2c, NULL) == -1)
                    {
                        Logger::getInstance().log(LogLevel::Error, "Failed to remove client from epoll");
                        return 1;
                    }

                    close(s2c);
                } else if(auto *packet = dynamic_cast<ConnReqPacket*>(packetRecvd.get()))
                {
                    auto event = std::make_unique<ConnectReqEvent>(client_addr.sin_addr.s_addr, packet->name);

                    pushEvent(std::ref(sharedResources), std::move(event));
                }
            }
        }
    }

    close(list_sock);

    Logger::getInstance().log(LogLevel::Debug, "Shutting down network loop");

    return 0;
}
