#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <string>

#include "logger.h"
#include "ntwk.h"

int main()
{
    int clientSocket = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(8080);
    serverAddress.sin_addr.s_addr = INADDR_ANY;

    /*std::string s;

    std::cout << "Send this: ";
    std::cin >> s;*/

    Logger::getInstance().log(LogLevel::Info, "Attempting to connect... ");

    if (connect(clientSocket, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) == -1)
    {
        Logger::getInstance().log(LogLevel::Error, "Failed to connect to server");
        return 1;
    }

    Logger::getInstance().log(LogLevel::Info, "Connected");

    ConnReqPacket packet;
    packet.name = "Charles Packet";
    packet.placeHolder = 10;

    Logger::getInstance().log(LogLevel::Info, "Sending packet");
    sendPacket(clientSocket, 1, packet);
    Logger::getInstance().log(LogLevel::Info, "Sent");

    std::string buffer;
    std::cin >> buffer;

    close(clientSocket);
}
