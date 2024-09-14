#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <string>

#include "logger.h"
#include "network/udp_network.h"
#include "event.h"

int main()
{
    int clientSocket = socket(AF_INET, SOCK_DGRAM, 0);

    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(8080);
    serverAddress.sin_addr.s_addr = INADDR_ANY;

    /*std::string s;

    std::cout << "Send this: ";
    std::cin >> s;*/

    Logger::getInstance().log(LogLevel::Info, "Attempting to connect... ");

    /*
    if (connect(clientSocket, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) == -1)
    {
        Logger::getInstance().log(LogLevel::Error, "Failed to connect to server");
        return 1;
    }*/

    Logger::getInstance().log(LogLevel::Info, "Connected");

    ConnReqPacket packet;
    packet.name = "Charles Packet";
    packet.placeHolder = 10;

    Logger::getInstance().log(LogLevel::Info, "Sending packet");
    sendUDPPacket(clientSocket, serverAddress, CONN_REQ_PACKET_ID, packet);
    Logger::getInstance().log(LogLevel::Info, "Sent");

    std::string buffer;
    std::cin >> buffer;

    DisconnectClientPacket discPacket;
    packet.placeHolder = 11;

    Logger::getInstance().log(LogLevel::Info, "Sending disc packet");
    sendUDPPacket(clientSocket, serverAddress, DISC_CLI_PACKET_ID, discPacket);
    Logger::getInstance().log(LogLevel::Info, "Sent");

    std::cin >> buffer;

    close(clientSocket);
}
