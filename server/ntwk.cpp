#include <functional>
#include <iostream>
#include <sys/socket.h>
#include <sys/eventfd.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <cstring>
#include <memory>
#include <mutex>

#include "ntwk.h"
#include "event.h"

#define LISTEN_BACKLOG 5
#define MAX_EVENTS 10

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
    std::cout << "Starting network main" << std::endl;

    sharedNetResources.eventfd = eventfd(0, 0);
    int _eventfd = sharedNetResources.eventfd;

    struct epoll_event ev, events[MAX_EVENTS];
    int conn_sock, nfds, epollfd;
    int msgs_recvd = 0;

    int list_sock = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);

    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(8080);
    serverAddress.sin_addr.s_addr = INADDR_ANY;

    sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    if (bind(list_sock, reinterpret_cast<struct sockaddr *>(&serverAddress), sizeof(serverAddress)) == -1)
    {
        std::cout << "Error binding server socket" << std::endl;
        return 1;
    }

    listen(list_sock, LISTEN_BACKLOG);

    epollfd = epoll_create1(0); // Essentially same as create
    if (epollfd == -1)
    {
        std::cout << "Error creating epoll file descriptor" << std::endl;
        return 1;
    }

    ev.events = EPOLLIN;
    ev.data.fd = list_sock;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, list_sock, &ev) == -1)
    {
        std::cout << "Error epoll_ctl" << std::endl;
        return 1;
    }

    ev.events = EPOLLIN;
    ev.data.fd = _eventfd;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, _eventfd, &ev) == -1)
    {
        std::cout << "Error epoll_ctl" << std::endl;
        return 1;
    }

    // Main network loop
    for(;;)
    {
        if (shutdownFlag == true)
            break;
        // Timeout here to avoid busy wait
        // Eventually may want network events too so no indef wait
        nfds = epoll_wait(epollfd, events, MAX_EVENTS, 0);
        if (nfds == -1)
        {
            std::cout << "Error epoll_wait" << std::endl;
            return 1;
        }

        for(int n = 0; n < nfds; ++n)
        {
            // Listening socket active, likely connection attempt
            if (events[n].data.fd == list_sock)
            {
                conn_sock = accept(list_sock, (struct sockaddr*) &client_addr, &client_addr_len);
                if (conn_sock == -1) {
                    std::cout << "Error accepting client" << std::endl;
                    return 1;
                }

                std::cout << "Client connected from: " << client_addr.sin_addr.s_addr << std::endl;

                setnonblocking(conn_sock);
                ev.events = EPOLLIN;
                ev.data.fd = conn_sock;

                if (epoll_ctl(epollfd, EPOLL_CTL_ADD, conn_sock, &ev) == -1)
                {
                    std::cout << "Failed to add client to epoll" << std::endl;
                    return 1;
                }

            } else if (events[n].data.fd == _eventfd) {
                uint64_t u;
                read(_eventfd, &u, sizeof(uint64_t));

                std::unique_ptr<BaseEvent> event;
                {
                    std::lock_guard lock(sharedNetResources.queueMutex);
                    event = std::move(sharedNetResources.eventQueue.front());
                    sharedNetResources.eventQueue.pop();
                }

                switch(event->eventType) {
                    case EventType::EventTypeMessage: {
                        auto *eventData = static_cast<MessageEvent*>(event.get());
                        std::cout << "Event loop data: " << eventData->message << std::endl;
                        break;
                    }
                    default:
                        std::cout << "Default case" << std::endl;
                        break;
                }

            } else {
                // Client data
                char buffer[1024] = {0};
                int amt_recvd = recv(events[n].data.fd, buffer, sizeof(buffer), 0);

                // Client socket is closed
                if (amt_recvd == 0) 
                {
                    msgs_recvd++;
                    std::cout << "Clients finished count: " << msgs_recvd << std::endl;

                    int s2c = events[n].data.fd;

                    std::cout << "Client socket closing" << std::endl;

                    if (epoll_ctl(epollfd, EPOLL_CTL_DEL, s2c, NULL) == -1)
                    {
                        std::cout << "Failed to add client to epoll" << std::endl;
                        return 1;
                    }

                    close(s2c);

                    // An error has occured on the socket
                } else if (amt_recvd == -1) {
                    std::cout << "Error recieving from client" << std::endl;

                    return 1;

                    // Data is ready and waiting on the socket
                } else {
                    int socketId = events[n].data.fd;
                    std::string strBuffer(buffer);

                    // We only need to define event in the branching; unless auto prevents?
                    if (strBuffer != "/SHUTIT") {
                        auto event = std::make_unique<MessageEvent>(socketId, strBuffer);

                        {
                            std::lock_guard<std::mutex> lock(sharedResources.queueMutex);
                            sharedResources.eventQueue.push(std::move(event));
                        }
                        sharedResources.eventCondition.notify_one();

                    } else {
                        auto event = std::make_unique<ShutdownEvent>(socketId);

                        {
                            std::lock_guard<std::mutex> lock(sharedResources.queueMutex);
                            sharedResources.eventQueue.push(std::move(event));
                        }
                        sharedResources.eventCondition.notify_one();
                    }
                }
            }
        }
    }

    close(list_sock);

    std::cout << "Let's start things basic - (network shutdown)" << std::endl;

    return 0;
}
