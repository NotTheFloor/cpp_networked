#include "event.h"


void pushEvent(SharedResources &sharedResources, std::unique_ptr<BaseEvent>(event))
{
    std::lock_guard<std::mutex> lock(sharedResources.queueMutex);
    sharedResources.eventQueue.push(std::move(event));

    sharedResources.eventCondition.notify_one();
}

