#pragma once

#include <windows.h>
#include <cstdint>

namespace GPxTrack
{
    struct GPxOverrideEntry
    {
        std::uint8_t laps;
        std::uint8_t flags;
        std::uint8_t pad[2]{};
    };

    // write-from-memory path
    extern std::uint8_t* g_pMagicWriteMem;
    extern std::uint8_t* g_pMagicResumeMem;

    // write-from-.dat path
    extern std::uint8_t* g_pMagicWriteDat;
    extern std::uint8_t* g_pMagicResumeDat;

    // common staging pointer [9D12B74]
    extern std::uint32_t* g_pMagicGlobal;

    extern GPxOverrideEntry g_GPxOverride[17];

    void InstallMagicHooks();
}
