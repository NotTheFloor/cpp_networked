#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <fcntl.h>

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

int main()
{
    struct epoll_event ev, events[MAX_EVENTS];
    int conn_sock, nfds, epollfd;

    int serverSocket = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);

    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(8080);
    serverAddress.sin_addr.s_addr = INADDR_ANY;

    sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    if (bind(serverSocket, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) == -1)
    {
        std::cout << "Error binding server socket" << std::endl;
        return 1;
    }

    listen(serverSocket, LISTEN_BACKLOG);

    epollfd = epoll_create1(0); // Essentially same as create
    if (epollfd == -1)
    {
        std::cout << "Error creating epoll file descriptor" << std::endl;
        return 1;
    }

    ev.events = EPOLLIN;
    ev.data.fd = serverSocket;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, serverSocket, &ev) == -1)
    {
        std::cout << "Error epoll_ctl" << std::endl;
        return 1;
    }

    for(;;)
    {
        nfds = epoll_wait(epollfd, events, MAX_EVENTS, -1);
        if (nfds == -1)
        {
            std::cout << "Error epoll_wait" << std::endl;
            return 1;
        }

        for(int n = 0; n < nfds; ++n)
        {
            if (events[n].data.fd == serverSocket)
            {
                conn_sock = accept(serverSocket, (struct sockaddr*) &client_addr, &client_addr_len);
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
                char buffer[1024] = {0};
                int amt_recvd = recv(events[n].data.fd, buffer, sizeof(buffer), 0);

                if (amt_recvd == 0) 
                {
                    int s2c = events[n].data.fd;

                    std::cout << "Client socket closing" << std::endl;

                    if (epoll_ctl(epollfd, EPOLL_CTL_DEL, s2c, NULL) == -1)
                    {
                        std::cout << "Failed to add client to epoll" << std::endl;
                        return 1;
                    }

                    close(s2c);

                } else if (amt_recvd == -1) {
                    std::cout << "Error recieving from client" << std::endl;

                    return 1;

                } else {
                    std::cout << "Message from client: " << buffer << std::endl;
                }
            }
        }
    }

    /*

    int clientSocket = accept(serverSocket, nullptr, nullptr);
    std::cout << "Client connected" << std::endl;

    char buffer[1024] = {0};
    recv(clientSocket, buffer, sizeof(buffer), 0);

    std::cout << "Message from client: " << buffer << std::endl;
    */

    close(serverSocket);

    std::cout << "Let's start things basic" << std::endl;

    return 0;
}
