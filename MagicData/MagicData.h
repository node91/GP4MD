#pragma once
#include <Windows.h>
#include <cstdint>
#include <cstddef>
#include <string>
#include "../Core/GP4Addresses.h"

namespace MagicData
{
    constexpr int   TRACK_COUNT = 17;
    constexpr int   DESC_COUNT = 139;
    constexpr DWORD BASE_TRACK1_ADDR = GP4Addresses::BASE_TRACK1_ADDR;

    enum class DescType { SETUP_BYTE, U8, U16, U32 };

    struct DescInfo
    {
        DescType    type{};
        std::size_t offset{};
        const char* comment{};
    };

    struct MagicBlockLayout
    {
        std::uint8_t* origBase = nullptr; // original GP4 magicdata base
        std::uint8_t* base = nullptr; // relocated magicdata base
        std::uint8_t* bumpStart = nullptr;
        std::uint8_t* bumpEnd = nullptr;
        std::size_t   bumpSize = 0;
        bool          valid = false;
    };

    extern std::uint8_t* g_LapTable;      // relocated lap table (used everywhere)
    extern std::uint8_t* g_LapTableOrig;  // original GP4 lap table (for reference)
    extern bool          g_EnableLogging;
    extern bool          g_LogDefaults;

    extern MagicBlockLayout g_Layout[TRACK_COUNT];
    extern DescInfo         g_Desc[DESC_COUNT];
    extern int              g_CurrentTrackIndex;

    void BeginDefaultsFile(const std::string& folder);
    void EndDefaultsFile();
    void WriteDefaultTrack(int trackIndex, std::uint8_t* descBase);

    void InitDescTable();
    bool PatchAllTracks();
}
