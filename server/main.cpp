#include <iostream>
#include <thread>

#include "ntwk.h"

int main()
{
    // Create network thread
    std::cout << "Creating network thread... ";
    std::thread network_thread(network_main);
    std::cout << "created" << std::endl;

    std::cout << "Waiting for network thread to finish... " << std::endl;
    network_thread.join();

    std::cout << "finished" << std::endl;

    return 1;
}
