#include "MagicData_Internal.h"
#include "MagicData.h"
#include "../Core/Encoding.h"
#include "../Core/Logging.h"
#include "../GP4MemLib/GP4MemLib.h"
#include "../IniLib/IniLib.h"

namespace MagicDataInternal
{
    using namespace MagicData;
    using namespace GP4MemLib;
    using namespace IniLib;

    // Scan a track's magicdata block to find the bump region boundaries.
    // For tracks 1–16, the bump region ends at 0xFF 0xFF 0x00 0x00.
    // For track 17, it ends at 0xFF 0xFF.
    MagicBlockLayout Scan(std::uint8_t* base, int trackIndex)
    {
        MagicBlockLayout L{};
        L.base = base;

        const std::size_t lastDescEnd = g_Desc[DESC_COUNT - 1].offset + 2;
        L.bumpStart = base + lastDescEnd;

        std::uint8_t* p = L.bumpStart;
        std::uint8_t* end = base + 0x20000; // safety upper bound

        if (trackIndex < TRACK_COUNT - 1)
        {
            // Tracks 1–16: look for FF FF 00 00
            while (p + 3 < end)
            {
                if (p[0] == 0xFF && p[1] == 0xFF &&
                    p[2] == 0x00 && p[3] == 0x00)
                {
                    L.bumpEnd = p + 4;
                    L.bumpSize = static_cast<std::size_t>(p - L.bumpStart);
                    L.valid = true;
                    return L;
                }
                p += 2;
            }
        }
        else
        {
            // Track 17: look for FF FF
            while (p + 1 < end)
            {
                if (p[0] == 0xFF && p[1] == 0xFF)
                {
                    L.bumpEnd = p + 2;
                    L.bumpSize = static_cast<std::size_t>(p - L.bumpStart);
                    L.valid = true;
                    return L;
                }
                p += 2;
            }
        }

        // L.valid remains false if no terminator found
        return L;
    }

    void PatchDesc(std::uint8_t* base, int descIndex, int value)
    {
        const DescInfo& D = g_Desc[descIndex - 1];
        std::uint8_t* addr = base + D.offset;

        switch (D.type)
        {
        case DescType::SETUP_BYTE:
        {
            const std::uint8_t b = EncodeSetupByte(value);
            MemUtils::patchAddress(addr, MemUtils::toBytes(b), 1, false);
            break;
        }
        case DescType::U8:
        {
            const std::uint8_t b = static_cast<std::uint8_t>(value);
            MemUtils::patchAddress(addr, MemUtils::toBytes(b), 1, false);
            break;
        }
        case DescType::U16:
        {
            const std::uint16_t v = static_cast<std::uint16_t>(value);
            MemUtils::patchAddress(addr, MemUtils::toBytes(v), 2, false);
            break;
        }
        case DescType::U32:
        {
            const std::uint32_t v = static_cast<std::uint32_t>(value);
            MemUtils::patchAddress(addr, MemUtils::toBytes(v), 4, false);
            break;
        }
        }
    }

    void PatchTrack(std::uint8_t* base,
        const IniLib::IniFile& trackIni,
        const IniLib::IniFile& globalIni,
        int trackIndex)
    {
        char section[32];
        std::snprintf(section, sizeof(section), "Track%02d", trackIndex + 1);

        if (!trackIni.hasSection(section))
            return;

        // Descriptor overrides: desc1..desc139
        for (int d = 1; d <= DESC_COUNT; ++d)
        {
            char key[16];
            std::snprintf(key, sizeof(key), "desc%d", d);

            if (!trackIni.hasKey(section, key))
                continue;

            IniLib::IniValue v = trackIni.get(section, key);
            if (v.length() == 0)
                continue;

            PatchDesc(base, d, v.getAs<int>());
        }

        // Lap overrides (with SprintRace awareness)
        bool sprintRace = false;
        if (globalIni.hasSection("RaceSettings") &&
            globalIni.hasKey("RaceSettings", "SprintRace"))
        {
            sprintRace = globalIni.get("RaceSettings", "SprintRace").getAs<int>() == 1;
        }

        if (trackIni.hasKey(section, "laps"))
        {
            IniLib::IniValue v = trackIni.get(section, "laps");

            // When SprintRace=1 and laps is empty, we leave laps to RaceSettings logic.
            if (sprintRace && v.length() == 0)
                return;

            if (v.length() > 0)
            {
                const std::uint8_t b = static_cast<std::uint8_t>(v.getAs<int>());
                std::uint8_t* addr = g_LapTable + trackIndex;
                MemUtils::patchAddress(addr, MemUtils::toBytes(b), 1, false);
            }
        }
    }
}
