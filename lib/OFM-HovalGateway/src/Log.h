#pragma once

// Seam for classes that only need OpenKNX's logging macros (logInfoP,
// logErrorP, logDebugP, logTraceP, ...) and not the rest of the OpenKNX
// facade (Arduino, knx stack, GPIO, flash, ...).
//
// Production build: forwards to the real OpenKNX.h.
// Native unit tests: test/stubs/Log.h is placed earlier on the include path
// and shadows this file, providing a lightweight stand-in for the macros.
#include "OpenKNX.h"
