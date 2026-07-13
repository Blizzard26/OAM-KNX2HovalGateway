#pragma once

// Native-test double for lib/OFM-HovalGateway/src/Log.h.
//
// Provides just enough of the OpenKNX logging macro surface (see
// lib/OGM-Common/src/OpenKNX/Log/Logger.h) for classes that only log to
// compile and run off-target, without pulling in Arduino or the knx stack.
// Picked up instead of the real Log.h because test_platformio.ini puts
// this directory earlier on the include path.

#include <cstddef>
#include <cstdint>
#include <cstdio>

#define logError(prefix, fmt, ...) fprintf(stderr, "[ERROR][%s] " fmt "\n", prefix, ##__VA_ARGS__)
#define logErrorP(fmt, ...) fprintf(stderr, "[ERROR][%s] " fmt "\n", logPrefix().c_str(), ##__VA_ARGS__)

#define logInfo(prefix, fmt, ...) fprintf(stdout, "[INFO][%s] " fmt "\n", prefix, ##__VA_ARGS__)
#define logInfoP(fmt, ...) fprintf(stdout, "[INFO][%s] " fmt "\n", logPrefix().c_str(), ##__VA_ARGS__)

#define logDebug(prefix, fmt, ...) fprintf(stdout, "[DEBUG][%s] " fmt "\n", prefix, ##__VA_ARGS__)
#define logDebugP(fmt, ...) fprintf(stdout, "[DEBUG][%s] " fmt "\n", logPrefix().c_str(), ##__VA_ARGS__)

#define logTrace(prefix, fmt, ...) fprintf(stdout, "[TRACE][%s] " fmt "\n", prefix, ##__VA_ARGS__)
#define logTraceP(fmt, ...) fprintf(stdout, "[TRACE][%s] " fmt "\n", logPrefix().c_str(), ##__VA_ARGS__)

#define logIndentUp() ((void)0)
#define logIndentDown() ((void)0)

inline void hovalTestLogHexDump(const char* prefix, const uint8_t* data, size_t size)
{
    fprintf(stdout, "[HEX][%s] ", prefix);
    for (size_t i = 0; i < size; ++i)
        fprintf(stdout, "%02X ", data[i]);
    fprintf(stdout, "\n");
}

#define logHexInfo(prefix, data, size) hovalTestLogHexDump(prefix, (const uint8_t*)(data), (size_t)(size))
#define logHexDebug(prefix, data, size) hovalTestLogHexDump(prefix, (const uint8_t*)(data), (size_t)(size))
