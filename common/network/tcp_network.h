#pragma once

#include <atomic>
#include <cstring>

#include "network/network_packets.h"
#include "event.h"

int tcp_network_main(SharedResources &sharedResources, SharedNetResources &sharedNetResources, std::atomic<bool> &shutdownFlag);

