#pragma once
#include <cstdint>

// All GP4 / GPxTrack version-dependent addresses and RVAs live here.
// This makes it obvious what must be revalidated when the EXE or GPxTrack changes.

namespace GP4Addresses
{
    // Base address of track 1 magicdata in GP4.exe
    constexpr std::uint32_t BASE_TRACK1_ADDR = 0x0062D328;

    // Original GP4 lap table address
    constexpr std::uint32_t LAP_TABLE_ADDR = 0x0062EADE;
}

namespace GPxTrackAddresses
{
    // RVAs inside gpxtrack.gxm (module base + RVA = absolute address)
    constexpr std::uint32_t RVA_MagicWriteMem = 0x00002D9F;
    constexpr std::uint32_t RVA_MagicResumeMem = 0x00002DA5;

    constexpr std::uint32_t RVA_MagicWriteDat = 0x00002E8A;
    constexpr std::uint32_t RVA_MagicResumeDat = 0x00002E90;

    // staging pointer [9D12B74] is ALSO inside gpxtrack.gxm
    constexpr std::uint32_t RVA_MagicGlobal = 0x00012B74;

    // Lap override hook
    constexpr std::uint32_t RVA_LapOverrideStart = 0x00004051; // mov eax,[ebp-10]
    constexpr std::uint32_t RVA_LapOverrideTarget = 0x00004091; // label 9D04091
}
