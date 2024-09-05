#pragma once

#include <atomic>
#include <cstring>

#include "network_packets.h"
#include "event.h"

struct SharedNetResources : SharedResources {
   int eventfd;
};

int network_main(SharedResources &sharedResources, SharedNetResources &sharedNetResources, std::atomic<bool> &shutdownFlag);

