#pragma once
#include <cstdint>
#include "MagicData.h"
#include "../IniLib/IniLib.h"

namespace MagicDataInternal
{
    MagicData::MagicBlockLayout Scan(std::uint8_t* base, int trackIndex);

    void PatchDesc(std::uint8_t* base, int descIndex, int value);

    void PatchTrack(std::uint8_t* base,
        const IniLib::IniFile& trackIni,
        const IniLib::IniFile& globalIni,
        int trackIndex);
}
