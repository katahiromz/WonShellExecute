#pragma once

#include <assert.h>

#define ASSERT assert

#define DebugPrintf printf

#ifndef DebugPrintf
    #define DebugPrintf DebugPrintf
    static inline void DebugPrintf(const char *fmt, ...)
    {
        va_list va;
        char buf[1024];
        va_start(va, fmt);
        StringCchVPrintfA(buf, _countof(buf), fmt, va);
        OutputDebugStringA(buf);
        va_end();
    }
#endif

#define TRACE(fmt, ...) \
    DebugPrintf("%s (%d): %s: " fmt, __FILE__, __LINE__, __func__, ##__VA_ARGS__)

#define ERR TRACE
