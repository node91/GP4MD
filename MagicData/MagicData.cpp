#include <windows.h>
#include <cstdio>
#include <vector>
#include <string>
#include <cstring>
#include <array>

#include "MagicData.h"
#include "MagicData_IO.h"
#include "MagicData_Internal.h"
#include "../Core/Logging.h"
#include "../Core/Encoding.h"
#include "../RaceSettings/RaceSettings.h"
#include "../GPxTrack/GPxTrack.h"
#include "../IniLib/IniLib.h"
#include "../GP4MemLib/GP4MemLib.h"
#include "../Core/GP4Addresses.h"

using namespace IniLib;
using namespace GP4MemLib;

namespace MagicData
{
    bool            g_EnableLogging = true;
    bool            g_LogDefaults = false;
    MagicBlockLayout g_Layout[TRACK_COUNT];
    DescInfo         g_Desc[DESC_COUNT];
    std::uint8_t* g_LapTable = nullptr; // relocated lap table
    std::uint8_t* g_LapTableOrig = nullptr; // original GP4 lap table
    int              g_CurrentTrackIndex = -1;

    // Static arena for all relocated magicdata + laps
    // 32 KB should be enough for all tracks + lap table
    static std::array<std::uint8_t, 0x5000> g_StaticArena{};

    // -------------------------------------------------------------------------
    // Internal helpers
    // -------------------------------------------------------------------------
    static void WriteLapTableToGP4()
    {
        // Original GP4 lap table location
        auto* dst = MemUtils::addressToPtr<std::uint8_t>(GP4Addresses::LAP_TABLE_ADDR);

        for (int t = 0; t < TRACK_COUNT; ++t)
        {
            const std::uint8_t val = g_LapTable[t];
            dst[t] = val;

            Logging::LogMD("WriteLapTable GP4[%02d] = %u (addr=%p)\n",
                t + 1, val, dst + t);
        }
    }

    // -------------------------------------------------------------------------
    // Descriptor table initialization
    // -------------------------------------------------------------------------
    void InitDescTable()
    {
        std::size_t off = 0;
        auto add = [&](int idx, DescType t, std::size_t size, const char* comment)
            {
                g_Desc[idx - 1] = { t, off, comment };
                off += size;
            };

        // 1–24: setup bytes
        add(1, DescType::SETUP_BYTE, 1, "Front wing [CC dry setup]");
        add(2, DescType::SETUP_BYTE, 1, "Rear wing");
        add(3, DescType::SETUP_BYTE, 1, "1st gear");
        add(4, DescType::SETUP_BYTE, 1, "2nd");
        add(5, DescType::SETUP_BYTE, 1, "3rd");
        add(6, DescType::SETUP_BYTE, 1, "4th");
        add(7, DescType::SETUP_BYTE, 1, "5th");
        add(8, DescType::SETUP_BYTE, 1, "6th");

        add(9, DescType::SETUP_BYTE, 1, "Front wing [CC wet setup]");
        add(10, DescType::SETUP_BYTE, 1, "Rear wing");
        add(11, DescType::SETUP_BYTE, 1, "1st gear");
        add(12, DescType::SETUP_BYTE, 1, "2nd");
        add(13, DescType::SETUP_BYTE, 1, "3rd");
        add(14, DescType::SETUP_BYTE, 1, "4th");
        add(15, DescType::SETUP_BYTE, 1, "5th");
        add(16, DescType::SETUP_BYTE, 1, "6th");

        add(17, DescType::SETUP_BYTE, 1, "Front wing [Player dry setup]");
        add(18, DescType::SETUP_BYTE, 1, "Rear wing");
        add(19, DescType::SETUP_BYTE, 1, "1st gear");
        add(20, DescType::SETUP_BYTE, 1, "2nd");
        add(21, DescType::SETUP_BYTE, 1, "3rd");
        add(22, DescType::SETUP_BYTE, 1, "4th");
        add(23, DescType::SETUP_BYTE, 1, "5th");
        add(24, DescType::SETUP_BYTE, 1, "6th");

        // 25: U32
        add(25, DescType::U32, 4, "Dry brake balance");

        // 26–33: setup bytes
        add(26, DescType::SETUP_BYTE, 1, "Front wing [Player wet setup]");
        add(27, DescType::SETUP_BYTE, 1, "Rear wing");
        add(28, DescType::SETUP_BYTE, 1, "1st gear");
        add(29, DescType::SETUP_BYTE, 1, "2nd");
        add(30, DescType::SETUP_BYTE, 1, "3rd");
        add(31, DescType::SETUP_BYTE, 1, "4th");
        add(32, DescType::SETUP_BYTE, 1, "5th");
        add(33, DescType::SETUP_BYTE, 1, "6th");

        // 34: U32
        add(34, DescType::U32, 4, "Wet brake balance");

        // 35–36: U8
        add(35, DescType::U8, 1, "Softer tyre [52 Hard, 53 Medium, 54 Soft, 55 Supersoft]");
        add(36, DescType::U8, 1, "Harder tyre [52 Hard, 53 Medium, 54 Soft, 55 Supersoft]");

        // 37: U16
        add(37, DescType::U16, 2, ">= 50 AI chooses softer tyre, otherwise harder");

        // 38–41: U16
        add(38, DescType::U16, 2, "");
        add(39, DescType::U16, 2, "");
        add(40, DescType::U16, 2, "");
        add(41, DescType::U16, 2, "");

        // 42–46: U16
        add(42, DescType::U16, 2, "Track grip");
        add(43, DescType::U16, 2, "");
        add(44, DescType::U16, 2, "");
        add(45, DescType::U16, 2, "Temperature (deg C) - warmer = higher top speed, less grip (17–31)");
        add(46, DescType::U16, 2, "Air Pressure (hPa) – affects PLAYER downforce & drag");

        // 47–73: U16
        add(47, DescType::U16, 2, "Engine power output (altitude simulation)");
        add(48, DescType::U16, 2, "Fuel per lap (2979 = 1 kg/lap)");
        add(49, DescType::U16, 2, "CC yield (higher = faster start)");
        add(50, DescType::U16, 2, "Player tyre wear factor");
        add(51, DescType::U16, 2, "CC aggressiveness min (Braking Range)");
        add(52, DescType::U16, 2, "CC aggressiveness max (Braking Range)");

        add(53, DescType::U16, 2, "[Ace] CC power factor");
        add(54, DescType::U16, 2, "[Ace] CC grip factor");
        add(55, DescType::U16, 2, "[Pro] CC power factor");
        add(56, DescType::U16, 2, "[Pro] CC grip factor");
        add(57, DescType::U16, 2, "[Semi-Pro] CC power factor");
        add(58, DescType::U16, 2, "[Semi-Pro] CC grip factor");
        add(59, DescType::U16, 2, "[Amateur] CC power factor");
        add(60, DescType::U16, 2, "[Amateur] CC grip factor");
        add(61, DescType::U16, 2, "[Rookie] CC power factor");
        add(62, DescType::U16, 2, "[Rookie] CC grip factor");

        add(63, DescType::U16, 2, "CC random performance range min");
        add(64, DescType::U16, 2, "CC random performance range max");
        add(65, DescType::U16, 2, "CC error chance");
        add(66, DescType::U16, 2, "CC recovery sectors");

        add(67, DescType::U16, 2, "Sectors to pit-in begin");
        add(68, DescType::U16, 2, "Sectors to pit-out end");
        add(69, DescType::U16, 2, "Pre-pit speed limit");

        add(70, DescType::U16, 2, "Fuel consumption Player");
        add(71, DescType::U16, 2, "Fuel consumption CC");
        add(72, DescType::U16, 2, "Tyre wear");
        add(73, DescType::U16, 2, "Sector where AI stop being cautious on lap 1");

        // 74–77: U32
        add(74, DescType::U32, 4, "Hotseat turn duration (70800 = 1:10.800)");
        add(75, DescType::U32, 4, "Real-time factor (+/-1000 = +/-1.0)");
        add(76, DescType::U32, 4, "Tyre change decision factor (dry↔wet)");
        add(77, DescType::U32, 4, "Same as above?");
        add(78, DescType::U16, 2, "Rain chance");

        // 79–81: U16
        add(79, DescType::U16, 2, "Segment start for pit in/out surface detection");
        add(80, DescType::U16, 2, "Segment end for pit in/out surface detection");
        add(81, DescType::U16, 2, "CC race grip (always 256)");

        // 82–101: U32
        add(82, DescType::U32, 4, "Time duration (ms) related to pits");
        add(83, DescType::U32, 4, "Segment where player starts in quicklaps");
        add(84, DescType::U32, 4, "Black Flag penalty (ms) (10000–30000)");
        add(85, DescType::U32, 4, "Black Flag severity (1024 = 80 kph)");
        add(86, DescType::U32, 4, "Wet CC engine mapping");
        add(87, DescType::U32, 4, "Wet CC tyre wear & grip factor (higher = more)");
        add(88, DescType::U32, 4, "Wet CC tyre wear factor (lower = more)");
        add(89, DescType::U32, 4, "Wet CC grip factor (lower = more)");
        add(90, DescType::U32, 4, "\"Handbrake\" – stop car moving in garage");
        add(91, DescType::U32, 4, "Car depth in garage (Player)");
        add(92, DescType::U32, 4, "Car depth in garage (AI)");
        add(93, DescType::U32, 4, "Car orientation in garage");
        add(94, DescType::U32, 4, "Pitstop stall depth from pitlane");
        add(95, DescType::U32, 4, "Pitstop stall depth finetune");
        add(96, DescType::U32, 4, "Tyre wear multiplier dry-soft (16384 = 100%)");
        add(97, DescType::U32, 4, "Tyre wear multiplier dry-hard (16384 = 100%)");
        add(98, DescType::U32, 4, "Tyre wear multiplier intermediate (16384 = 100%)");
        add(99, DescType::U32, 4, "Tyre wear multiplier wet-soft (16384 = 100%)");
        add(100, DescType::U32, 4, "Tyre wear multiplier wet-hard (16384 = 100%)");
        add(101, DescType::U32, 4, "Tyre wear multiplier monsoon (16384 = 100%)");

        // 102–139: U16
        add(102, DescType::U16, 2, "Pitstop group 1 %");
        add(103, DescType::U16, 2, "Stop 1");
        add(104, DescType::U16, 2, "Pit window 1");
        add(105, DescType::U16, 2, "");
        add(106, DescType::U16, 2, "");
        add(107, DescType::U16, 2, "");
        add(108, DescType::U16, 2, "");
        add(109, DescType::U16, 2, "");

        add(110, DescType::U16, 2, "Pitstop group 2 %");
        add(111, DescType::U16, 2, "Stop 1");
        add(112, DescType::U16, 2, "Pit window 1");
        add(113, DescType::U16, 2, "Stop 2");
        add(114, DescType::U16, 2, "Pit window 2");
        add(115, DescType::U16, 2, "");
        add(116, DescType::U16, 2, "");
        add(117, DescType::U16, 2, "");

        add(118, DescType::U16, 2, "Pitstop group 3 %");
        add(119, DescType::U16, 2, "Stop 1");
        add(120, DescType::U16, 2, "Pit window 1");
        add(121, DescType::U16, 2, "Stop 2");
        add(122, DescType::U16, 2, "Pit window 2");
        add(123, DescType::U16, 2, "Stop 3");
        add(124, DescType::U16, 2, "Pit window 3");
        add(125, DescType::U16, 2, "");

        add(126, DescType::U16, 2, "Failure chance: suspension");
        add(127, DescType::U16, 2, "Failure chance: loose wheel");
        add(128, DescType::U16, 2, "Failure chance: puncture");
        add(129, DescType::U16, 2, "Failure chance: engine");
        add(130, DescType::U16, 2, "Failure chance: transmission");
        add(131, DescType::U16, 2, "Failure chance: oil/water leak");
        add(132, DescType::U16, 2, "Failure chance: throttle/brake");
        add(133, DescType::U16, 2, "Failure chance: electrics");

        add(134, DescType::U16, 2, "");
        add(135, DescType::U16, 2, "");
        add(136, DescType::U16, 2, "");
        add(137, DescType::U16, 2, "");

        add(138, DescType::U16, 2, "Bump factor");
        add(139, DescType::U16, 2, "Bump shift");
    }

    // -------------------------------------------------------------------------
    // PatchAllTracks
    // -------------------------------------------------------------------------
    bool PatchAllTracks()
    {
        InitDescTable();

        // 1) Scan original GP4 layout to discover structure only
        auto* base = MemUtils::addressToPtr<std::uint8_t>(BASE_TRACK1_ADDR);

        for (int t = 0; t < TRACK_COUNT; ++t)
        {
            g_Layout[t].origBase = base;

            MagicBlockLayout L = MagicDataInternal::Scan(base, t);
            if (!L.valid)
            {
                Logging::LogMD("Track %02d scan failed\n", t + 1);
                return false;
            }

            g_Layout[t].bumpStart = L.bumpStart;
            g_Layout[t].bumpEnd = L.bumpEnd;
            g_Layout[t].bumpSize = L.bumpSize;
            g_Layout[t].valid = true;

            base = L.bumpEnd;
        }

        // After last track, GP4's original lap table starts here
        g_LapTableOrig = base;

        // 2) Compute relocated sizes inside static arena
        const std::size_t lastDescEnd = g_Desc[DESC_COUNT - 1].offset + 2;
        std::size_t       trackSize[TRACK_COUNT] = {};
        std::size_t       totalSize = 0;

        for (int t = 0; t < TRACK_COUNT; ++t)
        {
            const std::size_t bumpBytes =
                static_cast<std::size_t>(g_Layout[t].bumpEnd - g_Layout[t].bumpStart);

            trackSize[t] = lastDescEnd + bumpBytes + 0x20;
            totalSize += trackSize[t];
        }

        totalSize += TRACK_COUNT; // relocated lap table

        if (totalSize > g_StaticArena.size())
        {
            Logging::LogMD("Static arena too small (needed=%zu, have=%zu)\n",
                totalSize, g_StaticArena.size());
            return false;
        }

        // 3) Assign relocated bases and relocated lap table
        std::uint8_t* arena = g_StaticArena.data();
        std::uint8_t* p = arena;

        for (int t = 0; t < TRACK_COUNT; ++t)
        {
            g_Layout[t].base = p;
            p += trackSize[t];
        }

        g_LapTable = p;

        // 4) Initialize relocated lap table, preferring laps from .dat when present
        for (int t = 0; t < TRACK_COUNT; ++t)
        {
            std::uint8_t baseLap = *(g_LapTableOrig + t);

            std::vector<std::uint8_t> dat;
            int datLaps = 0;
            if (LoadDatFile(t, dat) && ExtractLapsFromDat(dat, datLaps))
            {
                int tmp = datLaps;
                if (tmp < 1)   tmp = 1;
                if (tmp > 255) tmp = 255;
                baseLap = static_cast<std::uint8_t>(tmp);

                Logging::LogMD("Track %02d laps from .dat = %d\n", t + 1, datLaps);
            }

            *(g_LapTable + t) = baseLap;
        }

        // 5) Resolve folder and global INI
        std::string folder;
        {
            char path[MAX_PATH]{};
            HMODULE hMod = nullptr;
            GetModuleHandleExA(
                GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                reinterpret_cast<LPCSTR>(&PatchAllTracks),
                &hMod
            );
            GetModuleFileNameA(hMod, path, MAX_PATH);
            std::string full(path);
            const std::size_t pos = full.find_last_of("\\/");
            folder = full.substr(0, pos + 1);
        }

        IniFile globalIni;
        const bool hasGlobal = globalIni.load(folder + "GP4MD.ini");

        if (hasGlobal && globalIni.hasSection("General") &&
            globalIni.hasKey("General", "Log"))
        {
            g_EnableLogging = globalIni.get("General", "Log").getAs<int>() != 0;
        }

        if (hasGlobal && globalIni.hasSection("General") &&
            globalIni.hasKey("General", "LogDefaults"))
        {
            g_LogDefaults = globalIni.get("General", "LogDefaults").getAs<int>() != 0;
        }

        BeginDefaultsFile(folder);

        // 6) Build relocated magicdata blocks per track
        for (int t = 0; t < TRACK_COUNT; ++t)
        {
            std::uint8_t* dstBase = g_Layout[t].base;
            std::uint8_t* origBase = g_Layout[t].origBase;

            // 6a) Start with original descriptor region as fallback
            std::memcpy(dstBase, origBase, lastDescEnd);

            // 6b) Try to load .dat magicdata and overwrite descriptor + bump region
            std::vector<std::uint8_t> dat;
            std::uint8_t* datMd = nullptr;
            std::size_t   datMdSize = 0;
            bool          hasDatMagic = false;

            if (LoadDatFile(t, dat))
            {
                datMd = FindMagicDataInDat(dat, datMdSize);
                if (datMd)
                {
                    Logging::LogMD("Track %2d: using .dat magicdata (size=%zu)\n",
                        t + 1, datMdSize);

                    // Copy descriptor region from .dat (clamped to descriptor size)
                    std::size_t payload = datMdSize;
                    if (payload > lastDescEnd)
                        payload = lastDescEnd;

                    std::memcpy(dstBase, datMd, payload);
                    hasDatMagic = true;
                }
            }

            // 6c) Copy bump region: prefer .dat bump region when .dat magicdata is present
            const std::size_t bumpBytesOrig =
                static_cast<std::size_t>(g_Layout[t].bumpEnd - g_Layout[t].bumpStart);

            std::uint8_t* dstBumpStart = dstBase + lastDescEnd;

            if (hasDatMagic && datMdSize > lastDescEnd)
            {
                // .dat layout: descriptor [0..lastDescEnd), bump [lastDescEnd..terminator]
                const std::size_t datBumpBytes = datMdSize - lastDescEnd;

                std::memcpy(dstBumpStart, datMd + lastDescEnd, datBumpBytes);

                g_Layout[t].bumpStart = dstBumpStart;
                g_Layout[t].bumpEnd = dstBumpStart + datBumpBytes;
                g_Layout[t].bumpSize = datBumpBytes;
            }
            else
            {
                // Fallback: original GP4 bump region
                std::memcpy(dstBumpStart, g_Layout[t].bumpStart, bumpBytesOrig);

                g_Layout[t].bumpStart = dstBumpStart;
                g_Layout[t].bumpEnd = dstBumpStart + bumpBytesOrig;
                g_Layout[t].bumpSize = bumpBytesOrig;
            }

            // 6d) Dump defaults for this track (if enabled)
            WriteDefaultTrack(t, dstBase);

            // 6e) Track INI + RaceSettings on relocated block
            char file[64];
            std::snprintf(file, sizeof(file), "Track%02d.ini", t + 1);
            IniFile ini;

            if (ini.load(folder + file))
            {
                MagicDataInternal::PatchTrack(dstBase, ini, globalIni, t);

                if (hasGlobal)
                    ApplyRaceSettings(globalIni, ini, t);
            }
        }

        EndDefaultsFile();

        // 7) Log relocated lap table
        for (int i = 0; i < TRACK_COUNT; ++i)
        {
            std::uint8_t* addr = g_LapTable + i;
            const std::uint8_t val = *addr;

            Logging::LogMD("Track %02d relocLaps=%u addr=%p\n",
                i + 1, val, addr);
        }

        WriteLapTableToGP4();
        return true;
    }
}
