#include "MagicData_IO.h"
#include "MagicData.h"
#include <cstring>
#include <windows.h>
#include <cstdio>

namespace MagicData
{
    std::string GetGP4RootFolder()
    {
        char path[MAX_PATH]{};
        HMODULE hExe = GetModuleHandleA(nullptr);
        if (!hExe)
            return {};

        GetModuleFileNameA(hExe, path, MAX_PATH);
        std::string full(path);
        const std::size_t pos = full.find_last_of("\\/");
        if (pos == std::string::npos)
            return {};

        return full.substr(0, pos + 1);
    }

    bool LoadDatFile(int trackIndex, std::vector<std::uint8_t>& out)
    {
        const std::string root = GetGP4RootFolder();
        if (root.empty())
            return false;

        char name[32];
        std::snprintf(name, sizeof(name), "Circuits\\S1CT%02d.DAT", trackIndex + 1);
        const std::string fullPath = root + name;

        std::FILE* f = nullptr;
        if (fopen_s(&f, fullPath.c_str(), "rb") != 0 || !f)
            return false;

        std::fseek(f, 0, SEEK_END);
        const long size = std::ftell(f);
        std::fseek(f, 0, SEEK_SET);

        if (size <= 0)
        {
            std::fclose(f);
            return false;
        }

        out.resize(static_cast<std::size_t>(size));
        std::fread(out.data(), 1, static_cast<std::size_t>(size), f);
        std::fclose(f);

        return true;
    }

    std::uint8_t* FindMagicDataInDat(std::vector<std::uint8_t>& dat,
        std::size_t& outSize)
    {
        auto* base = dat.data();
        const std::size_t size = dat.size();

        std::size_t start = static_cast<std::size_t>(-1);
        for (std::size_t i = 0; i + 4 <= size; ++i)
        {
            if (base[i] == 'M' && base[i + 1] == 'A' &&
                base[i + 2] == '0' && base[i + 3] == '3')
            {
                start = i + 4;
                break;
            }
        }
        if (start == static_cast<std::size_t>(-1))
            return nullptr;

        std::size_t end = static_cast<std::size_t>(-1);
        for (std::size_t i = start; i + 3 <= size; ++i)
        {
            if (base[i] == 0x00 && base[i + 1] == 0xFF && base[i + 2] == 0xFF)
            {
                end = i + 3;
                break;
            }
        }
        if (end == static_cast<std::size_t>(-1) || end <= start)
            return nullptr;

        outSize = end - start;
        return base + start;
    }

    void CopyDatMagicDataToGP4(std::uint8_t* src,
        std::size_t size,
        std::uint8_t* dst,
        int trackIndex)
    {
        if (size < 3)
            return;

        const std::size_t payload = size - 3;
        std::memcpy(dst, src, payload);

        if (trackIndex < 16)
        {
            dst[payload + 0] = 0xFF;
            dst[payload + 1] = 0xFF;
            dst[payload + 2] = 0x00;
            dst[payload + 3] = 0x00;
        }
        else
        {
            dst[payload + 0] = 0xFF;
            dst[payload + 1] = 0xFF;
        }
    }

    bool ExtractLapsFromDat(const std::vector<std::uint8_t>& dat,
        int& outLaps)
    {
        outLaps = 0;
        if (dat.empty())
            return false;

        static constexpr std::uint8_t marker[] = {
            0x6C, 0x61, 0x70, 0x73, 0x7C // "laps|"
        };
        static constexpr std::size_t markerLen = sizeof(marker);

        const std::uint8_t* base = dat.data();
        const std::size_t size = dat.size();

        // search from the end for the marker
        std::size_t pos = static_cast<std::size_t>(-1);
        for (std::size_t i = size; i-- > markerLen - 1; )
        {
            if (std::memcmp(base + i - (markerLen - 1), marker, markerLen) == 0)
            {
                pos = i - (markerLen - 1);
                break;
            }
        }

        if (pos == static_cast<std::size_t>(-1))
            return false;

        std::size_t p = pos + markerLen;
        if (p >= size)
            return false;

        int  value = 0;
        bool hasDigit = false;

        // read ASCII digits after "laps|"
        while (p < size)
        {
            const std::uint8_t c = base[p];
            if (c < '0' || c > '9')
                break;

            hasDigit = true;
            value = value * 10 + (c - '0');
            ++p;
        }

        if (!hasDigit)
            return false;

        outLaps = value;
        return true;
    }
}
