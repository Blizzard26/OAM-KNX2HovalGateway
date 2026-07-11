#pragma once

#include "CanTransport.h"
#include <memory>

// Confines all platform #ifdef selection to CanTransportFactory.cpp.
// Reads pins/config from hardware.h.
std::unique_ptr<CanTransport> createCanTransport();
