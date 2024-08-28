#include <iostream>
#include <thread>
#include <memory>
#include <mutex>

#include "event.h"
#include "ntwk.h"

int main()
{
    SharedResources sharedResources;

    // Create network thread
    std::cout << "Creating network thread... ";
    std::thread network_thread(network_main, std::ref(sharedResources));
    std::cout << "created" << std::endl;

    while(true) {
        std::unique_ptr<BaseEvent> event;
        {
            std::unique_lock<std::mutex> lock(sharedResources.queueMutex);
            sharedResources.eventCondition.wait(lock, [&sharedResources]{ return !sharedResources.eventQueue.empty(); });
            event = std::move(sharedResources.eventQueue.front());
            sharedResources.eventQueue.pop();
        }

        switch(event->eventType) {
            case EventType::EventTypeMessage:
                auto *eventData = static_cast<MessageEvent*>(event.get());
                std::cout << "Event loop data: " << eventData->message << std::endl;
        }
    }

    std::cout << "Waiting for network thread to finish... " << std::endl;
    network_thread.join();

    std::cout << "finished" << std::endl;

    return 1;
}
