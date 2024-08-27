#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <cstring>

#include "ntwk.h"

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

int network_main()
{
    std::cout << "Starting network main" << std::endl;

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

    if (bind(list_sock, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) == -1)
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

    // Main network loop
    for(;;)
    {
        nfds = epoll_wait(epollfd, events, MAX_EVENTS, 0);
        if (nfds == -1)
        {
            std::cout << "Error epoll_wait" << std::endl;
            return 1;
        }

        if (msgs_recvd > 3 && nfds == 0)
        {
            std::cout << "Shutting down" << std::endl;
            break;
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
                    std::cout << "Message from client: " << buffer << std::endl;
                }
            }
        }
    }

    close(list_sock);

    std::cout << "Let's start things basic" << std::endl;

    return 0;
}
