#pragma once

#include <atomic>

#include "event.h"

void repeatTimerThread(const std::string &threadName, const int tickRate_ms, SharedResources &sharedResources, std::atomic<bool> &shutdownFlag); 
void fireOnceTimerThread(const std::string &threadName, const int tickRate_ms, SharedResources &sharedResources); 
     
