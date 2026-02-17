#pragma once
#include "../IniLib/IniLib.h"

void ApplyRaceSettings(const IniLib::IniFile& raceIni,
    const IniLib::IniFile& trackIni,
    int trackIndex);
