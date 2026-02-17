#include "GPxTrack.h"
#include "../MagicData/MagicData.h"
#include "../Core/Logging.h"
#include "../Core/GP4Addresses.h"

static const int kMagicBlockLayoutSize = sizeof(MagicData::MagicBlockLayout);

namespace GPxTrack
{
    // resolved dynamically from gpxtrack.gxm base
    std::uint8_t* g_pMagicWriteMem = nullptr;
    std::uint8_t* g_pMagicResumeMem = nullptr;

    std::uint8_t* g_pMagicWriteDat = nullptr;
    std::uint8_t* g_pMagicResumeDat = nullptr;

    // staging pointer is ALSO inside gpxtrack.gxm → now resolved dynamically
    std::uint32_t* g_pMagicGlobal = nullptr;

    GPxOverrideEntry g_GPxOverride[MagicData::TRACK_COUNT] = {};
}

// -----------------------------------------------------------------------------
// Track index resolver
// -----------------------------------------------------------------------------
namespace
{
    int GetTrackIndexFromBase(std::uint8_t* ptr)
    {
        for (int t = 0; t < MagicData::TRACK_COUNT; ++t)
        {
            std::uint8_t* base = MagicData::g_Layout[t].origBase;
            if (base == ptr)
                return t;
        }
        return -1;
    }

    std::uint32_t GetRelocatedMagicPtr(std::uint8_t* orig)
    {
        using namespace MagicData;

        const int t = GetTrackIndexFromBase(orig);
        if (t < 0 || t >= TRACK_COUNT)
            return reinterpret_cast<std::uint32_t>(orig);

        MagicBlockLayout& lay = g_Layout[t];
        if (!lay.valid || !lay.base)
            return reinterpret_cast<std::uint32_t>(orig);

        return reinterpret_cast<std::uint32_t>(lay.base);
    }

    void PatchJump(void* src, void* dst)
    {
        DWORD oldProt{};
        auto* p = static_cast<std::uint8_t*>(src);

        if (!VirtualProtect(p, 5, PAGE_EXECUTE_READWRITE, &oldProt))
            return;

        const auto rel = reinterpret_cast<std::uint8_t*>(dst) - (p + 5);

        p[0] = 0xE9;
        *reinterpret_cast<std::int32_t*>(p + 1) = static_cast<std::int32_t>(rel);

        DWORD dummy{};
        VirtualProtect(p, 5, oldProt, &dummy);
    }

    void PatchLapOverride(std::uint8_t* base)
    {
        using namespace GPxTrackAddresses;

        std::uint8_t* pStart = base + RVA_LapOverrideStart;
        std::uint8_t* pTarget = base + RVA_LapOverrideTarget;

        DWORD oldProt{};
        if (!VirtualProtect(pStart, 5, PAGE_EXECUTE_READWRITE, &oldProt))
            return;

        // jmp pTarget
        pStart[0] = 0xE9;
        *reinterpret_cast<std::int32_t*>(pStart + 1) =
            static_cast<std::int32_t>(pTarget - (pStart + 5));

        DWORD dummy{};
        VirtualProtect(pStart, 5, oldProt, &dummy);
    }

    bool ResolveGPxTrackAddresses()
    {
        using namespace GPxTrackAddresses;

        HMODULE hGPx = nullptr;
        while (!(hGPx = GetModuleHandleA("gpxtrack.gxm")))
            Sleep(100);

        auto* base = reinterpret_cast<std::uint8_t*>(hGPx);

        auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE)
            return false;

        auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE)
            return false;

        GPxTrack::g_pMagicWriteMem = base + RVA_MagicWriteMem;
        GPxTrack::g_pMagicResumeMem = base + RVA_MagicResumeMem;

        GPxTrack::g_pMagicWriteDat = base + RVA_MagicWriteDat;
        GPxTrack::g_pMagicResumeDat = base + RVA_MagicResumeDat;

        GPxTrack::g_pMagicGlobal =
            reinterpret_cast<std::uint32_t*>(base + RVA_MagicGlobal);

        PatchLapOverride(base);
        return true;
    }
}

// -----------------------------------------------------------------------------
// Hook 1: memory path
// -----------------------------------------------------------------------------
void __declspec(naked) GPxMagicHook_Mem()
{
    __asm {
        pushad

        mov  eax, esi
        push eax
        call GetTrackIndexFromBase
        add  esp, 4

        mov  MagicData::g_CurrentTrackIndex, eax

        mov  eax, esi
        push eax
        call GetRelocatedMagicPtr
        add  esp, 4

        mov  edx, GPxTrack::g_pMagicGlobal
        mov[edx], eax

        popad

        mov  eax, GPxTrack::g_pMagicResumeMem
        jmp  eax
    }
}

// -----------------------------------------------------------------------------
// Hook 2: .dat path
// -----------------------------------------------------------------------------
void __declspec(naked) GPxMagicHook_Dat()
{
    __asm {
        pushad

        mov  eax, MagicData::g_CurrentTrackIndex
        cmp  eax, 0
        jl   no_map
        cmp  eax, MagicData::TRACK_COUNT
        jge  no_map

        mov  ecx, eax
        mov  edx, OFFSET MagicData::g_Layout
        mov  eax, kMagicBlockLayoutSize
        imul ecx, eax
        add  edx, ecx
        mov  eax, [edx + 4]
        jmp  write_ptr

        no_map :
        mov  eax, edx

            write_ptr :
        mov  edx, GPxTrack::g_pMagicGlobal
            mov[edx], eax

            popad

            mov  eax, GPxTrack::g_pMagicResumeDat
            jmp  eax
    }
}

// -----------------------------------------------------------------------------
// Install hooks
// -----------------------------------------------------------------------------
void GPxTrack::InstallMagicHooks()
{
    if (!ResolveGPxTrackAddresses())
    {
        Logging::LogMD("GPxTrack: InstallMagicHooks aborted\n");
        return;
    }

    PatchJump(g_pMagicWriteMem, GPxMagicHook_Mem);
    PatchJump(g_pMagicWriteDat, GPxMagicHook_Dat);
}
