#include <windows.h>
#include <string>

#include "MagicData/MagicData.h"
#include "GPxTrack/GPxTrack.h"
#include "RaceSettings/RaceSettings.h"
#include "IniLib/IniLib.h"
#include "Core/Logging.h"

DWORD WINAPI MainThread(LPVOID)
{
    // Wait until gpxtrack.gxm is loaded
    HMODULE hGPx = nullptr;
    while (!(hGPx = GetModuleHandleA("gpxtrack.gxm")))
        Sleep(100);

    // Optional: small delay to ensure module initialization
    Sleep(200);

    // Patch MagicData first
    if (!MagicData::PatchAllTracks())
        Logging::LogMD("PatchAllTracks failed\n");

    // Install GPxTrack hooks
    GPxTrack::InstallMagicHooks();

    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(hModule);

        HANDLE hThread = CreateThread(nullptr, 0, MainThread, nullptr, 0, nullptr);
        if (hThread)
            CloseHandle(hThread); // avoid handle leak
    }
    return TRUE;
}
