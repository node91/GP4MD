#include <windows.h>
#include <cstdio>

#include "RaceSettings.h"
#include "../MagicData/MagicData.h"
#include "../Core/Logging.h"
#include "../GP4MemLib/GP4MemLib.h"

using namespace IniLib;
using namespace GP4MemLib;
using namespace MagicData;

namespace
{
    inline bool PatchIfChanged16(std::uint8_t* addr,
        std::uint16_t newVal,
        const char* label,
        int trackIndex)
    {
        const auto oldVal = *reinterpret_cast<std::uint16_t*>(addr);
        if (oldVal == newVal)
            return false;

        MemUtils::patchAddress(addr, MemUtils::toBytes(newVal), 2, false);

        Logging::LogRS("Track %02d %s %u -> %u\n",
            trackIndex + 1, label, oldVal, newVal);
        return true;
    }

    inline bool PatchIfChanged8(std::uint8_t* addr,
        std::uint8_t newVal,
        const char* label,
        int trackIndex)
    {
        const auto oldVal = *addr;
        if (oldVal == newVal)
            return false;

        MemUtils::patchAddress(addr, MemUtils::toBytes(newVal), 1, false);

        Logging::LogRS("Track %02d %s %u -> %u\n",
            trackIndex + 1, label, oldVal, newVal);
        return true;
    }
}

void ApplyRaceSettings(const IniFile& raceIni,
    const IniFile& trackIni,
    int trackIndex)
{
    constexpr const char* raceSec = "RaceSettings";

    if (!raceIni.hasSection(raceSec))
        return;

    std::uint8_t* base = g_Layout[trackIndex].base;

    // ------------------------------------------------------------
    // SprintRace / SprintLaps
    // ------------------------------------------------------------
    int sprint = 0;
    if (raceIni.hasKey(raceSec, "SprintRace"))
    {
        IniValue v = raceIni.get(raceSec, "SprintRace");
        if (v.length() > 0)
            sprint = v.getAs<int>();
    }

    std::uint8_t* lapAddr = g_LapTable + trackIndex;
    const std::uint8_t rawLaps = *lapAddr;
    std::uint8_t newLaps = rawLaps;

    if (sprint == 1)
    {
        char trackSec[32];
        std::snprintf(trackSec, sizeof(trackSec), "Track%02d", trackIndex + 1);

        int sprintLaps = 0;
        if (trackIni.hasSection(trackSec) &&
            trackIni.hasKey(trackSec, "SprintLaps"))
        {
            IniValue v = trackIni.get(trackSec, "SprintLaps");
            if (v.length() > 0)
                sprintLaps = v.getAs<int>();
        }

        if (sprintLaps > 0)
        {
            int tmp = sprintLaps;
            if (tmp < 1)   tmp = 1;
            if (tmp > 255) tmp = 255;
            newLaps = static_cast<std::uint8_t>(tmp);
        }
        else
        {
            int tmp = rawLaps / 3;
            if (tmp < 1)   tmp = 1;
            if (tmp > 255) tmp = 255;
            newLaps = static_cast<std::uint8_t>(tmp);
        }
    }

    PatchIfChanged8(lapAddr, newLaps, "laps", trackIndex);

    // ------------------------------------------------------------
    // Sprint‑specific pitstop logic (only when SprintRace = 1)
    // ------------------------------------------------------------
    if (sprint == 1)
    {
        int pitStop = 0;
        if (raceIni.hasKey(raceSec, "SprintPitStop"))
        {
            IniValue v = raceIni.get(raceSec, "SprintPitStop");
            if (v.length() > 0)
                pitStop = v.getAs<int>();
        }

        if (pitStop == 1)
        {
            // desc102 = 100
            {
                const DescInfo& D = g_Desc[102 - 1];
                std::uint8_t* addr = base + D.offset;
                PatchIfChanged16(addr, 100, "desc102", trackIndex);
            }

            // desc110–114, 118–124 = 0
            const int zeroList[] = {
                110,111,112,113,114,
                118,119,120,121,122,123,124
            };

            for (int desc : zeroList)
            {
                const DescInfo& D = g_Desc[desc - 1];
                std::uint8_t* addr = base + D.offset;

                char label[16];
                std::snprintf(label, sizeof(label), "desc%d", desc);

                PatchIfChanged16(addr, 0, label, trackIndex);
            }

            // desc103 (SprintPitStopLap)
            {
                int pitLap = -1;

                if (raceIni.hasKey(raceSec, "SprintPitStopLap"))
                {
                    IniValue v = raceIni.get(raceSec, "SprintPitStopLap");
                    if (v.length() > 0)
                        pitLap = v.getAs<int>();
                }

                if (pitLap <= 0)
                {
                    const int laps = *lapAddr;
                    pitLap = (laps + 1) / 2;
                    if (pitLap < 1) pitLap = 1;
                }

                const DescInfo& D = g_Desc[103 - 1];
                std::uint8_t* addr = base + D.offset;
                PatchIfChanged16(addr,
                    static_cast<std::uint16_t>(pitLap),
                    "desc103",
                    trackIndex);
            }

            // desc104 (SprintPitStopWindow)
            {
                int pitWindow = 3;

                if (raceIni.hasKey(raceSec, "SprintPitStopWindow"))
                {
                    IniValue v = raceIni.get(raceSec, "SprintPitStopWindow");
                    if (v.length() > 0)
                        pitWindow = v.getAs<int>();
                }

                const DescInfo& D = g_Desc[104 - 1];
                std::uint8_t* addr = base + D.offset;
                PatchIfChanged16(addr,
                    static_cast<std::uint16_t>(pitWindow),
                    "desc104",
                    trackIndex);
            }
        }
    }

    // --------------------------------------------------------
    // FuelMultiplier (desc48, desc70, desc71)
    // --------------------------------------------------------
    {
        double fm = 1.0;
        bool   hasFm = false;

        if (raceIni.hasKey(raceSec, "FuelMultiplier"))
        {
            IniValue v = raceIni.get(raceSec, "FuelMultiplier");
            if (v.length() > 0)
            {
                fm = v.getAs<double>();
                hasFm = true;
            }
        }

        if (hasFm && fm != 1.0)
        {
            bool anyFuelChanged = false;

            auto applyFuelMul16 = [&](int desc)
                {
                    const DescInfo& D = g_Desc[desc - 1];
                    std::uint8_t* addr = base + D.offset;

                    const std::uint16_t raw =
                        *reinterpret_cast<std::uint16_t*>(addr);

                    double scaled = static_cast<double>(raw) * fm;
                    if (scaled > 65535.0) scaled = 65535.0;
                    if (scaled < 0.0)     scaled = 0.0;

                    const std::uint16_t newVal =
                        static_cast<std::uint16_t>(scaled);

                    char label[16];
                    std::snprintf(label, sizeof(label), "desc%d", desc);
                    if (PatchIfChanged16(addr, newVal, label, trackIndex))
                        anyFuelChanged = true;
                };

            applyFuelMul16(48);   // fuel per lap
            applyFuelMul16(70);   // fuel player
            applyFuelMul16(71);   // fuel CC

            if (anyFuelChanged)
            {
                Logging::LogRS("Track %02d FuelMultiplier applied = %.3f\n",
                    trackIndex + 1, fm);
            }
        }
    }

    // --------------------------------------------------------
    // TyreWearMultiplier (desc50, desc72)
    // --------------------------------------------------------
    {
        double tm = 1.0;
        bool   hasTm = false;

        if (raceIni.hasKey(raceSec, "TyreWearMultiplier"))
        {
            IniValue v = raceIni.get(raceSec, "TyreWearMultiplier");
            if (v.length() > 0)
            {
                tm = v.getAs<double>();
                hasTm = true;
            }
        }

        if (hasTm && tm != 1.0)
        {
            bool anyTyreChanged = false;

            auto applyTyreMul16 = [&](int desc)
                {
                    const DescInfo& D = g_Desc[desc - 1];
                    std::uint8_t* addr = base + D.offset;

                    const std::uint16_t raw =
                        *reinterpret_cast<std::uint16_t*>(addr);

                    double scaled = static_cast<double>(raw) * tm;
                    if (scaled > 65535.0) scaled = 65535.0;
                    if (scaled < 0.0)     scaled = 0.0;

                    const std::uint16_t newVal =
                        static_cast<std::uint16_t>(scaled);

                    char label[16];
                    std::snprintf(label, sizeof(label), "desc%d", desc);
                    if (PatchIfChanged16(addr, newVal, label, trackIndex))
                        anyTyreChanged = true;
                };

            applyTyreMul16(50);   // tyre wear player
            applyTyreMul16(72);   // tyre wear CC

            if (anyTyreChanged)
            {
                Logging::LogRS("Track %02d TyreWearMultiplier applied = %.3f\n",
                    trackIndex + 1, tm);
            }
        }
    }

    // --------------------------------------------------------
    // CCYield (desc49)
    // --------------------------------------------------------
    {
        int  yield = 0;
        bool hasYield = false;

        if (raceIni.hasKey(raceSec, "CCYield"))
        {
            IniValue v = raceIni.get(raceSec, "CCYield");
            if (v.length() > 0)
            {
                yield = v.getAs<int>();
                hasYield = true;
            }
        }

        if (hasYield)
        {
            const DescInfo& D = g_Desc[49 - 1];
            std::uint8_t* addr = base + D.offset;
            const std::uint16_t v = static_cast<std::uint16_t>(yield);

            if (PatchIfChanged16(addr, v, "desc49", trackIndex))
            {
                Logging::LogRS("RaceSettings: Track %02d CCYield changed to %d\n",
                    trackIndex + 1, yield);
            }
        }
    }

    // --------------------------------------------------------
    // CCStartCaution (desc73)
    // --------------------------------------------------------
    {
        int  caution = 0;
        bool hasCaution = false;

        if (raceIni.hasKey(raceSec, "CCStartCaution"))
        {
            IniValue v = raceIni.get(raceSec, "CCStartCaution");
            if (v.length() > 0)
            {
                caution = v.getAs<int>();
                hasCaution = true;
            }
        }

        if (hasCaution)
        {
            const DescInfo& D = g_Desc[73 - 1];
            std::uint8_t* addr = base + D.offset;
            const std::uint16_t v = static_cast<std::uint16_t>(caution);

            if (PatchIfChanged16(addr, v, "desc73", trackIndex))
            {
                Logging::LogRS("RaceSettings: Track %02d CCStartCaution changed to %d\n",
                    trackIndex + 1, caution);
            }
        }
    }
}
