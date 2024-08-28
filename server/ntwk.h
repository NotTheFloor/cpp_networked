#pragma once

#include <atomic>

#include "event.h"

int network_main(SharedResources &sharedResources, std::atomic<bool> &shutdownFlag);

