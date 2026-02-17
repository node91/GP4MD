#include <cstdio>
#include <string>
#include "MagicData.h"
#include "../Core/Logging.h"

namespace MagicData
{
    // This module is only used to extract baseline magicdata into defaults.ini
    // for analysis / template generation. It is fully controlled by g_LogDefaults.

    static std::FILE* g_DefaultsFile = nullptr;

    void BeginDefaultsFile(const std::string& folder)
    {
        if (!g_LogDefaults)
            return;

        const std::string path = folder + "defaults.ini";

        std::FILE* f = nullptr;
        if (fopen_s(&f, path.c_str(), "w") != 0 || !f)
        {
            Logging::LogMD("Could not open defaults.ini for writing\n");
            return;
        }

        g_DefaultsFile = f;
    }

    void EndDefaultsFile()
    {
        if (g_DefaultsFile)
            std::fclose(g_DefaultsFile);

        g_DefaultsFile = nullptr;
    }

    void WriteDefaultTrack(int trackIndex, std::uint8_t* descBase)
    {
        if (!g_LogDefaults || !g_DefaultsFile)
            return;

        std::fprintf(g_DefaultsFile, "[Track%02d]\n", trackIndex + 1);

        for (int d = 1; d <= DESC_COUNT; ++d)
        {
            const DescInfo& D = g_Desc[d - 1];
            std::uint8_t* addr = descBase + D.offset;

            int value = 0;
            switch (D.type)
            {
            case DescType::SETUP_BYTE:
                value = static_cast<int>(*addr) - 151;
                break;
            case DescType::U8:
                value = *addr;
                break;
            case DescType::U16:
                value = *reinterpret_cast<std::uint16_t*>(addr);
                break;
            case DescType::U32:
                value = *reinterpret_cast<std::uint32_t*>(addr);
                break;
            }

            std::fprintf(g_DefaultsFile, "desc%d = %d    ; %s\n",
                d, value, D.comment ? D.comment : "");
        }

        std::fprintf(g_DefaultsFile, "\n");
    }
}
