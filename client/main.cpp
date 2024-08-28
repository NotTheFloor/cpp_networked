#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <string>

int main()
{
    int clientSocket = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(8080);
    serverAddress.sin_addr.s_addr = INADDR_ANY;

    std::string s;

    std::cout << "Send this: ";
    std::cin >> s;

    connect(clientSocket, (struct sockaddr *)&serverAddress, sizeof(serverAddress));

    const char *message = s.c_str();
    send(clientSocket, message, std::strlen(message), 0);

    close(clientSocket);
}
