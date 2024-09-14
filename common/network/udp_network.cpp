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
#include "network/tcp_network.h"
#include "event.h"

#define LISTEN_BACKLOG 5
#define MAX_EVENTS 10
#define MAX_CONN_ATTEMPTS 3


// We start swapping to camelCase here... fine
int udp_network_main(SharedResources &sharedResources, SharedNetResources &sharedNetResources, std::atomic<bool> &shutdownFlag)
{
    int nextClientId = 0;
    Logger::getInstance().log(LogLevel::Debug, "Starting network thread");

    // Note: we'll use contain which is C++21
    // We should either use boost or containerize this in a wrapper class
    std::unordered_map<uint32_t, int> pendingUserMapByAddrIp, connUserMapByAddrIp;
    std::unordered_map<int, sockaddr_in> pendingUserMapById, connUserMapById;
    sharedNetResources.eventfd = eventfd(0, 0);
    int _eventfd = sharedNetResources.eventfd;

    struct epoll_event ev, events[MAX_EVENTS];
    int nfds, epollfd;

    int serverSocket = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, 0);

    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(8080);
    serverAddress.sin_addr.s_addr = INADDR_ANY;

    // I think we need to move this into the loop scope
    sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    if (bind(serverSocket, reinterpret_cast<struct sockaddr *>(&serverAddress), sizeof(serverAddress)) == -1)
    {
        Logger::getInstance().log(LogLevel::Error, "Error binding server socket");
        return 1;
    }

    epollfd = epoll_create1(0); // Essentially same as create
    if (epollfd == -1)
    {
        Logger::getInstance().log(LogLevel::Error, "Error creating epoll file descriptor");
        return 1;
    }

    ev.events = EPOLLIN;
    ev.data.fd = serverSocket;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, serverSocket, &ev) == -1)
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
        nfds = epoll_wait(epollfd, events, MAX_EVENTS, -1);
        if (nfds == -1)
        {
            Logger::getInstance().log(LogLevel::Error, "Error epoll_wait");
            return 1;
        }

        for(int n = 0; n < nfds; ++n)
        {
            /* // Listening socket active, likely connection attempt
            if (events[n].data.fd == list_sock)
            {
                conn_sock = accept(list_sock, (struct sockaddr*) &client_addr, &client_addr_len);
                if (conn_sock == -1) {
                    Logger::getInstance().log(LogLevel::Error, "Error accepting client");
                    return 1;
                }

                Logger::getInstance().log(LogLevel::Info, std::string("Client connected from: ") + inet_ntoa(client_addr.sin_addr));

                // If anyone in git notices the missing, I'm so sorry
                //conn_sock = setnonblocking(conn_sock);
                ev.events = EPOLLIN | EPOLLHUP | EPOLLRDHUP;
                ev.data.fd = conn_sock;

                if (epoll_ctl(epollfd, EPOLL_CTL_ADD, conn_sock, &ev) == -1)
                {
                    Logger::getInstance().log(LogLevel::Error, "Failed to add client to epoll");
                    return 1;
                }

                // Pending event loop
            } else */
            if (events[n].data.fd == _eventfd) {
                uint64_t u;
                read(_eventfd, &u, sizeof(uint64_t));

                std::unique_ptr<BaseEvent> event;
                {
                    std::lock_guard<std::mutex> lock(sharedNetResources.queueMutex);
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
                        if (!pendingUserMapById.contains(eventData->uniqueId)) {
                            Logger::getInstance().log(LogLevel::Warning, "Accepted connection for non-pending user");
                            // Needs to be hadnled
                        }
                        if (connUserMapById.contains(eventData->clientId)) {
                            Logger::getInstance().log(LogLevel::Warning, "Accepted client already connected");
                        }

                        connUserMapById[eventData->clientId] = pendingUserMapById[eventData->uniqueId];
                        connUserMapByAddrIp[pendingUserMapById[eventData->uniqueId].sin_addr.s_addr] = eventData->clientId;
                        pendingUserMapByAddrIp.erase(pendingUserMapById[eventData->uniqueId].sin_addr.s_addr);
                        pendingUserMapById.erase(eventData->uniqueId);
                        
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
                sockaddr_in remoteAddr;
                std::unique_ptr<BasePacket> packetRecvd = recvUDPPacket(events[n].data.fd, packetFactory, remoteAddr);

                if (packetRecvd == nullptr) 
                {
                    /*int s2c = events[n].data.fd;

                    Logger::getInstance().log(LogLevel::Info, "Client socket closing");

                    if (epoll_ctl(epollfd, EPOLL_CTL_DEL, s2c, NULL) == -1)
                    {
                        Logger::getInstance().log(LogLevel::Error, "Failed to remove client from epoll");
                        return 1;
                    }

                    close(s2c);*/

                    // What does remoteAddr equal here?
                    // NULLPTR means error, disconnect the client
                    if (connUserMapByAddrIp.contains(remoteAddr.sin_addr.s_addr)) {
                        auto event = std::make_unique<ClientDisconnectEvent>(connUserMapByAddrIp[remoteAddr.sin_addr.s_addr], DisconnectType::Ungraceful);

                        connUserMapById.erase(connUserMapByAddrIp[remoteAddr.sin_addr.s_addr]);
                        connUserMapByAddrIp.erase(remoteAddr.sin_addr.s_addr);
                        // We should also look at pending here. A problem would be if pending keeps getting bigger...

                        pushEvent(std::ref(sharedResources), std::move(event));
                    } else {
                        Logger::getInstance().log(LogLevel::Warning, "Client disconnected before being accepted");
                    }
                } else {
                    // We need to differentiate and check what packets can be handled based on connections state
                    if(auto *packet = dynamic_cast<ConnReqPacket*>(packetRecvd.get()))
                    {
                        //struct sockaddr_in addr;
                        //socklen_t addr_len = sizeof(addr);
                        char ip_str[INET_ADDRSTRLEN];

                        /*if (getpeername(events[n].data.fd, (struct sockaddr*)&addr, &addr_len) == -1) {
                            Logger::getInstance().log(LogLevel::Error, std::string("Error getting peer name: ") + strerror(errno));
                            return 1;
                        }*/

                        if (inet_ntop(AF_INET, &(remoteAddr.sin_addr), ip_str, INET_ADDRSTRLEN) == nullptr) {
                            Logger::getInstance().log(LogLevel::Error, std::string("Error getting ip using ntop: ") + strerror(errno));
                            return 1;
                        }

                        //uint32_t sck = events[n].data.fd;
                        nextClientId += 1;
                        auto event = std::make_unique<ConnectReqEvent>(nextClientId, packet->name, std::string(ip_str), std::to_string(ntohs(remoteAddr.sin_port)));

                        pushEvent(std::ref(sharedResources), std::move(event));

                        if (pendingUserMapByAddrIp.contains(remoteAddr.sin_addr.s_addr)) {
                            //pendingUserMap[events[n].data.fd] += 1;
                            // We need to detect ddos here?
                            Logger::getInstance().log(LogLevel::Warning, "Client attempted while still pending");
                        } else {
                            pendingUserMapByAddrIp[remoteAddr.sin_addr.s_addr] = nextClientId;
                            pendingUserMapById[nextClientId] = remoteAddr;
                        }
                    } else if(auto *packet = dynamic_cast<DisconnectClientPacket*>(packetRecvd.get())) {
                        if (connUserMapByAddrIp.contains(remoteAddr.sin_addr.s_addr)) {
                            // Need to test for pending here as well and handle appropraitely

                            auto event = std::make_unique<ClientDisconnectEvent>(connUserMapByAddrIp[remoteAddr.sin_addr.s_addr], DisconnectType::Graceful);
                        
                            connUserMapById.erase(connUserMapByAddrIp[remoteAddr.sin_addr.s_addr]);
                            connUserMapByAddrIp.erase(remoteAddr.sin_addr.s_addr);
                            // Need to check and handle pending vs connected clients

                            pushEvent(std::ref(sharedResources), std::move(event));
                        } else {
                            Logger::getInstance().log(LogLevel::Warning, "Disconnect packet received from not connected client");
                        }
                    } else {
                        Logger::getInstance().log(LogLevel::Warning, "Unknown packet type received");
                        // Should probably push packet type here
                    }
                }
            }
        }
    }

    close(serverSocket);

    Logger::getInstance().log(LogLevel::Debug, "Shutting down network loop");

    return 0;
}
