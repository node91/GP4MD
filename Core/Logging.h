#pragma once
#include <windows.h>
#include <cstdarg>
#include <cstdio>
#include "../MagicData/MagicData.h"

namespace Logging
{
    inline void LogV(const char* prefix, const char* fmt, va_list ap)
    {
        if (!MagicData::g_EnableLogging)
            return;

        char buf[512];

        int written = 0;
        if (prefix && *prefix)
        {
            written = std::snprintf(buf, sizeof(buf), "%s", prefix);
            if (written < 0 || static_cast<std::size_t>(written) >= sizeof(buf))
                return;
        }

        std::vsnprintf(buf + written, sizeof(buf) - written, fmt, ap);
        OutputDebugStringA(buf);
    }

    inline void LogMD(const char* fmt, ...)
    {
        va_list ap;
        va_start(ap, fmt);
        LogV("GP4MD MagicData: ", fmt, ap);
        va_end(ap);
    }

    inline void LogRS(const char* fmt, ...)
    {
        va_list ap;
        va_start(ap, fmt);
        LogV("GP4MD RaceSettings: ", fmt, ap);
        va_end(ap);
    }
}
